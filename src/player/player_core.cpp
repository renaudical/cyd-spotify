// The real player: a task on core 0 polls the music source chosen in settings (status, art, controls)
// and hands snapshots to update() on the UI core, which fills in the position between polls.
#ifndef FAKE_PLAYER

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "album_art.h"
#include "net/net.h"
#include "player.h"
#include "settings/settings.h"
#include "source.h"
#include "updater/updater.h"

namespace player {
namespace {

// Consecutive failed polls before the UI says the source can't be reached.
constexpr int kFailuresBeforeUnreachable = 3;
// How often to look again while there's nothing to talk to, or nobody signed in.
constexpr uint32_t kNotFoundRetryMs = 10000;
constexpr uint32_t kNeedsSetupRetryMs = 2000;
// A failed art download (weak Wi-Fi, a slow CDN) is retried this often, this many times per track.
constexpr uint32_t kArtRetryMs = 10000;
constexpr int kArtAttempts = 3;
// The UI lets go of the art within one Now Playing refresh (100 ms); this is a generous bound.
constexpr uint32_t kArtReleaseTimeoutMs = 2000;

enum class Reach : uint8_t { kConnecting, kOk, kUnreachable, kNotFound, kNeedsSetup };

// One art buffer (RAM is the constraint: see CLAUDE.md). The task only writes it while the UI isn't
// showing it, which costs nothing visible: a new track's art is withdrawn as soon as its URL changes
// anyway. Allocated in begin(), because 20 KB doesn't fit in the ESP32's static RAM segment.
constexpr size_t kArtBytes = album_art::kSize * album_art::kSize * sizeof(uint16_t);
uint16_t* art_pixels;
lv_image_dsc_t art_image;

// Written by the poll task, read by update(); guarded by lock.
struct Shared {
  Reach reach;
  uint32_t seq;  // bumped on every successful poll
  Poll poll;
  uint32_t art_seq;  // bumped whenever art changes
  bool art;          // the art buffer holds this track's art
  bool art_shown;    // the UI may be drawing the art buffer
};
Shared shared = {};
SemaphoreHandle_t lock;
QueueHandle_t commands;
Source* source = nullptr;

// Only touched by the poll task.
String art_url;
int art_attempts = 0;       // failed downloads of art_url
uint32_t art_retry_at = 0;  // millis() of the next attempt after a failure

// Only touched by update() on the UI core.
State current = {};
Poll latest = {};
uint32_t latest_seq = 0;
uint32_t art_seq = 0;
bool art = false;

void publishArt(bool ready) {
  xSemaphoreTake(lock, portMAX_DELAY);
  shared.art = ready;
  shared.art_seq++;
  xSemaphoreGive(lock);
}

bool waitForArtRelease() {
  for (uint32_t started = millis(); millis() - started < kArtReleaseTimeoutMs; vTaskDelay(pdMS_TO_TICKS(20))) {
    xSemaphoreTake(lock, portMAX_DELAY);
    bool shown = shared.art_shown;
    xSemaphoreGive(lock);
    if (!shown) return true;
  }
  return false;
}

// Follows the source's art URL rather than the title, since the art can lag the title change.
void updateArt() {
  String url = source->artUrl();
  if (url != art_url) {
    art_url = url;
    art_attempts = 0;
    publishArt(false);  // the old track's art is wrong now, even if the new one fails
  } else if (art_attempts == 0 || art_attempts >= kArtAttempts || (int32_t)(millis() - art_retry_at) < 0) {
    return;  // already showing it, given up on it, or waiting to retry
  }
  if (url.isEmpty()) return;

  // Two TLS sessions don't fit in RAM at once; the source reconnects on its next request.
  source->disconnect();
  uint32_t started = millis();
  if (waitForArtRelease() && album_art::fetch(url.c_str(), art_pixels)) {
    Serial.printf("art: loaded in %u ms (free heap %u, largest block %u)\n", (unsigned)(millis() - started),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    art_attempts = 0;
    publishArt(true);
  } else if (++art_attempts < kArtAttempts) {
    art_retry_at = millis() + kArtRetryMs;
  } else {
    Serial.println("art: giving up on this track");
  }
}

void setReach(Reach reach) {
  xSemaphoreTake(lock, portMAX_DELAY);
  shared.reach = reach;
  xSemaphoreGive(lock);
}

void task(void*) {
  Poll poll = {};
  int failures = 0;
  for (;;) {
    if (updater::due()) {
      source->disconnect();  // the check needs the RAM its TLS session holds
      updater::check();
    }
    source->service();

    uint32_t started = millis();
    uint32_t wait = source->pollIntervalMs(poll.status);
    if (net::wifi() != net::Wifi::kConnected) {
      failures = 0;
      setReach(Reach::kConnecting);
    } else {
      switch (source->poll(poll)) {
        case Source::Result::kOk:
          if (failures >= kFailuresBeforeUnreachable) Serial.printf("%s: reachable again\n", source->name());
          failures = 0;
          xSemaphoreTake(lock, portMAX_DELAY);
          shared.reach = Reach::kOk;
          shared.seq++;
          shared.poll = poll;
          xSemaphoreGive(lock);
          updateArt();
          break;
        case Source::Result::kFailed:
          if (++failures == kFailuresBeforeUnreachable) {
            Serial.printf("%s: unreachable\n", source->name());
            setReach(Reach::kUnreachable);
          }
          break;
        case Source::Result::kNotFound:
          setReach(Reach::kNotFound);
          wait = kNotFoundRetryMs;
          break;
        case Source::Result::kNeedsSetup:
          setReach(Reach::kNeedsSetup);
          wait = kNeedsSetupRetryMs;
          break;
      }
    }

    // Sleep until the next poll, but wake for a button press and poll right after it.
    uint32_t elapsed = millis() - started;
    Command command;
    if (xQueueReceive(commands, &command, pdMS_TO_TICKS(elapsed < wait ? wait - elapsed : 1)) &&
        command != Command::kWake && net::wifi() == net::Wifi::kConnected) {
      source->send(command, poll);
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
  switch (reach) {
    case Reach::kConnecting:
      return Link::kConnecting;
    case Reach::kUnreachable:
      return Link::kUnreachable;
    case Reach::kNotFound:
      return Link::kNotFound;
    case Reach::kNeedsSetup:
      return Link::kNeedsSetup;
    case Reach::kOk:
      break;
  }
  return Link::kOk;
}

void queue(Command command) { xQueueSend(commands, &command, 0); }

}  // namespace

void wake() { queue(Command::kWake); }

void copyText(char* out, size_t size, const char* text) {
  size_t n = strlen(text);
  if (n >= size) {
    n = size - 1;
    while (n > 0 && (static_cast<uint8_t>(text[n]) & 0xC0) == 0x80) n--;  // back to a character start
  }
  memcpy(out, text, n);
  out[n] = '\0';
}

void begin() {
  art_pixels = static_cast<uint16_t*>(malloc(kArtBytes));
  art_image.header.magic = LV_IMAGE_HEADER_MAGIC;
  art_image.header.cf = LV_COLOR_FORMAT_RGB565;
  art_image.header.w = album_art::kSize;
  art_image.header.h = album_art::kSize;
  art_image.header.stride = album_art::kSize * sizeof(uint16_t);
  art_image.data_size = kArtBytes;
  art_image.data = reinterpret_cast<const uint8_t*>(art_pixels);
  source = settings::source() == settings::Source::kSpotify ? spotifySource() : wiimSource();
  lock = xSemaphoreCreateMutex();
  commands = xQueueCreate(4, sizeof(Command));
  current.link = Link::kWifiConnecting;
  current.source = source->name();
  Serial.printf("player: source is %s\n", source->name());
  xTaskCreatePinnedToCore(task, "player", 12288, nullptr, 1, nullptr, 0);
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
      Serial.printf("player: %s \"%s\" by \"%s\"\n",
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
    current.art = art && latest.status != Status::kStopped ? &art_image : nullptr;
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

}  // namespace player

#endif  // FAKE_PLAYER
