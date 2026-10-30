// The real player: a task on core 0 talks to the WiiM over HTTPS (status, art, controls) and hands
// snapshots to update() on the UI core, which fills in the position between polls.
#ifndef FAKE_PLAYER

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <atomic>

#include "album_art.h"
#include "net/net.h"
#include "player.h"
#include "secrets.h"
#include "wiim_text.h"

namespace player {
namespace {

constexpr char kApiUrl[] = "https://" WIIM_HOST "/httpapi.asp?command=";
constexpr uint32_t kPollMs = 1000;
constexpr uint16_t kTimeoutMs = 3000;
// Consecutive failed polls before the UI says the WiiM can't be reached.
constexpr int kFailuresBeforeUnreachable = 3;
// A failed art download (weak Wi-Fi, a slow CDN) is retried this often, this many times per track.
constexpr uint32_t kArtRetryMs = 10000;
constexpr int kArtAttempts = 3;

enum class Reach : uint8_t { kConnecting, kOk, kUnreachable };
enum class Command : uint8_t { kTogglePlayPause, kNext, kPrevious };

struct Poll {
  Status status;
  char title[sizeof(State::title)];
  char artist[sizeof(State::artist)];
  char album[sizeof(State::album)];
  uint32_t position_ms;
  uint32_t duration_ms;
  uint32_t polled_at_ms;  // millis() when position_ms was read
};

// Two art buffers: the task only ever writes the one the UI isn't showing. Allocated in begin(),
// because 40 KB doesn't fit in the ESP32's static RAM segment.
constexpr size_t kArtBytes = album_art::kSize * album_art::kSize * sizeof(uint16_t);
uint16_t* art_pixels[2];
lv_image_dsc_t art_images[2];

// Written by the poll task, read by update(); guarded by lock.
struct Shared {
  Reach reach;
  uint32_t seq;  // bumped on every successful poll
  Poll poll;
  uint32_t art_seq;  // bumped whenever art changes
  int8_t art;        // art buffer to show, -1 for none
  int8_t art_shown;  // art buffer the UI is showing, -1 for none
};
Shared shared = {.art = -1, .art_shown = -1};
SemaphoreHandle_t lock;
QueueHandle_t commands;

// Only touched by the poll task.
WiFiClientSecure tls;
HTTPClient http;
String art_url;
int art_attempts = 0;       // failed downloads of art_url
uint32_t art_retry_at = 0;  // millis() of the next attempt after a failure

// Only touched by update() on the UI core.
State current = {};
Poll latest = {};
uint32_t latest_seq = 0;
uint32_t art_seq = 0;
int8_t art = -1;

// suspend()/resume() handshake with the poll task.
std::atomic<bool> suspend_requested{false};
std::atomic<bool> suspended{false};
constexpr uint32_t kSuspendTimeoutMs = 20000;  // a poll plus an art download, worst case

bool request(const char* command, String& reply) {
  String url = kApiUrl;
  url += command;
  if (!http.begin(tls, url)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("wiim: %s failed (%d %s)\n", command, code, http.errorToString(code).c_str());
    http.end();
    return false;
  }
  reply = http.getString();
  http.end();  // keeps the connection open for the next request (setReuse)
  return true;
}

Status parseStatus(const char* s) {
  // "load" is buffering the next track.
  if (!strcmp(s, "play") || !strcmp(s, "load")) return Status::kPlaying;
  if (!strcmp(s, "pause")) return Status::kPaused;
  return Status::kStopped;
}

bool fetchStatus(Poll& out) {
  String reply;
  if (!request("getPlayerStatus", reply)) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, reply);
  if (err) {
    Serial.printf("wiim: bad getPlayerStatus reply (%s)\n", err.c_str());
    return false;
  }
  out.status = parseStatus(doc["status"] | "");
  wiim_text::decode(doc["Title"] | "", out.title, sizeof(out.title));
  wiim_text::decode(doc["Artist"] | "", out.artist, sizeof(out.artist));
  wiim_text::decode(doc["Album"] | "", out.album, sizeof(out.album));
  out.position_ms = strtoul(doc["curpos"] | "0", nullptr, 10);
  out.duration_ms = strtoul(doc["totlen"] | "0", nullptr, 10);
  out.polled_at_ms = millis();
  return true;
}

void publishArt(int8_t buffer) {
  xSemaphoreTake(lock, portMAX_DELAY);
  shared.art = buffer;
  shared.art_seq++;
  xSemaphoreGive(lock);
}

// Follows getMetaInfo's albumArtURI rather than the title, since the art URL can lag the title change.
void updateArt() {
  String reply;
  if (!request("getMetaInfo", reply)) return;
  JsonDocument doc;
  if (deserializeJson(doc, reply)) return;
  String url = doc["metaData"]["albumArtURI"] | "";
  if (!url.startsWith("http")) url = "";  // e.g. "un_known" for sources without art
  if (url != art_url) {
    art_url = url;
    art_attempts = 0;
    publishArt(-1);  // the old track's art is wrong now, even if the new one fails
  } else if (art_attempts == 0 || art_attempts >= kArtAttempts || (int32_t)(millis() - art_retry_at) < 0) {
    return;  // already showing it, given up on it, or waiting to retry
  }
  if (url.isEmpty()) return;

  xSemaphoreTake(lock, portMAX_DELAY);
  int8_t target = shared.art_shown == 0 ? 1 : 0;
  xSemaphoreGive(lock);
  // Two TLS sessions don't fit in RAM at once; the WiiM connection reopens on the next request.
  tls.stop();
  uint32_t started = millis();
  if (album_art::fetch(url.c_str(), art_pixels[target])) {
    Serial.printf("art: loaded in %u ms (free heap %u, largest block %u)\n", (unsigned)(millis() - started),
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    art_attempts = 0;
    publishArt(target);
  } else if (++art_attempts < kArtAttempts) {
    art_retry_at = millis() + kArtRetryMs;
  } else {
    Serial.println("art: giving up on this track");
  }
}

void send(Command command) {
  static const char* const kCommands[] = {"setPlayerCmd:onepause", "setPlayerCmd:next", "setPlayerCmd:prev"};
  String reply;
  request(kCommands[static_cast<int>(command)], reply);
}

void setReach(Reach reach) {
  xSemaphoreTake(lock, portMAX_DELAY);
  shared.reach = reach;
  xSemaphoreGive(lock);
}

void task(void*) {
  tls.setInsecure();  // the WiiM only speaks HTTPS, with a self-signed certificate
  tls.setHandshakeTimeout(kTimeoutMs / 1000);
  http.setReuse(true);
  http.setConnectTimeout(kTimeoutMs);
  http.setTimeout(kTimeoutMs);

  Poll poll = {};
  int failures = 0;
  for (;;) {
    if (suspend_requested) {
      http.end();
      tls.stop();
      suspended = true;
      while (suspend_requested) vTaskDelay(pdMS_TO_TICKS(100));
      suspended = false;
    }

    uint32_t started = millis();
    if (net::wifi() != net::Wifi::kConnected) {
      failures = 0;
      setReach(Reach::kConnecting);
    } else if (fetchStatus(poll)) {
      if (failures >= kFailuresBeforeUnreachable) Serial.println("wiim: reachable again");
      failures = 0;
      xSemaphoreTake(lock, portMAX_DELAY);
      shared.reach = Reach::kOk;
      shared.seq++;
      shared.poll = poll;
      xSemaphoreGive(lock);
      updateArt();
    } else if (++failures == kFailuresBeforeUnreachable) {
      Serial.println("wiim: unreachable");
      setReach(Reach::kUnreachable);
    }

    // Sleep until the next poll, but wake for a button press and poll right after it.
    uint32_t elapsed = millis() - started;
    Command command;
    if (xQueueReceive(commands, &command, pdMS_TO_TICKS(elapsed < kPollMs ? kPollMs - elapsed : 1)) &&
        net::wifi() == net::Wifi::kConnected) {
      send(command);
    }
  }
}

Link currentLink(Reach reach) {
  switch (net::wifi()) {
    case net::Wifi::kConnecting:
      return Link::kWifiConnecting;
    case net::Wifi::kLost:
      return Link::kWifiLost;
    case net::Wifi::kConnected:
      break;
  }
  if (reach == Reach::kConnecting) return Link::kWiimConnecting;
  if (reach == Reach::kUnreachable) return Link::kWiimUnreachable;
  return Link::kOk;
}

void queue(Command command) { xQueueSend(commands, &command, 0); }

}  // namespace

void begin() {
  for (int i = 0; i < 2; i++) {
    art_pixels[i] = static_cast<uint16_t*>(malloc(kArtBytes));
    art_images[i].header.magic = LV_IMAGE_HEADER_MAGIC;
    art_images[i].header.cf = LV_COLOR_FORMAT_RGB565;
    art_images[i].header.w = album_art::kSize;
    art_images[i].header.h = album_art::kSize;
    art_images[i].header.stride = album_art::kSize * sizeof(uint16_t);
    art_images[i].data_size = kArtBytes;
    art_images[i].data = reinterpret_cast<const uint8_t*>(art_pixels[i]);
  }
  lock = xSemaphoreCreateMutex();
  commands = xQueueCreate(4, sizeof(Command));
  current.link = Link::kWifiConnecting;
  xTaskCreatePinnedToCore(task, "wiim", 12288, nullptr, 1, nullptr, 0);
}

void update() {
  xSemaphoreTake(lock, portMAX_DELAY);
  Reach reach = shared.reach;
  bool fresh = shared.seq != latest_seq;
  if (fresh) {
    latest = shared.poll;
    latest_seq = shared.seq;
  }
  bool art_changed = shared.art_seq != art_seq;
  if (art_changed) {
    art = shared.art_shown = shared.art;
    art_seq = shared.art_seq;
  }
  xSemaphoreGive(lock);

  Link link = currentLink(reach);
  if (art_changed || link != current.link || latest.status != current.status ||
      latest.duration_ms != current.duration_ms || strcmp(latest.title, current.title) ||
      strcmp(latest.artist, current.artist) || strcmp(latest.album, current.album)) {
    if (fresh && (latest.status != current.status || strcmp(latest.title, current.title))) {
      Serial.printf("wiim: %s \"%s\" by \"%s\"\n",
                    latest.status == Status::kPlaying  ? "playing"
                    : latest.status == Status::kPaused ? "paused"
                                                       : "stopped",
                    latest.title, latest.artist);
    }
    current.link = link;
    current.status = latest.status;
    current.duration_ms = latest.duration_ms;
    strlcpy(current.title, latest.title, sizeof(current.title));
    strlcpy(current.artist, latest.artist, sizeof(current.artist));
    strlcpy(current.album, latest.album, sizeof(current.album));
    current.art = art >= 0 && latest.status != Status::kStopped ? &art_images[art] : nullptr;
    current.version++;
  }

  uint32_t position = latest.position_ms;
  if (latest.status == Status::kPlaying) position += millis() - latest.polled_at_ms;
  if (latest.duration_ms && position > latest.duration_ms) position = latest.duration_ms;
  current.position_ms = position;
}

const State& state() { return current; }

void togglePlayPause() { queue(Command::kTogglePlayPause); }
void next() { queue(Command::kNext); }
void previous() { queue(Command::kPrevious); }

bool suspend() {
  suspend_requested = true;
  uint32_t started = millis();
  while (!suspended) {
    if (millis() - started > kSuspendTimeoutMs) {
      suspend_requested = false;
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  return true;
}

void resume() { suspend_requested = false; }

}  // namespace player

#endif  // FAKE_PLAYER
