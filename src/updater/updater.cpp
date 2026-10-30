#include "updater.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

#include <atomic>

#include "net/net.h"
#include "player/player.h"

// Set by the release workflow (-DFIRMWARE_VERSION=\"1.2.0\"); local builds are "dev".
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
// Forks publishing their own releases override this.
#ifndef UPDATE_REPO
#define UPDATE_REPO "renaudical/cyd-spotify"
#endif

// The Arduino core marks a freshly installed version valid at boot unless this returns true; we wait
// until it has proven it can reach GitHub (see confirmVersion()).
extern "C" bool verifyRollbackLater() { return true; }

namespace updater {
namespace {

constexpr char kLatestUrl[] = "https://api.github.com/repos/" UPDATE_REPO "/releases/latest";
// The release asset the device installs (the workflow also attaches a full image for the web installer).
constexpr char kAssetName[] = "cyd-spotify-ota.bin";
constexpr uint32_t kFirstCheckMs = 60 * 1000;  // after boot, once things have settled
constexpr uint32_t kCheckIntervalMs = 6 * 60 * 60 * 1000;
constexpr uint32_t kRetryMs = 30 * 60 * 1000;
constexpr uint16_t kTimeoutMs = 10000;

std::atomic<bool> idle{false};
std::atomic<bool> is_installing{false};
std::atomic<uint8_t> progress{0};
char target[16] = "";

struct Release {
  String version;  // tag name, e.g. "v1.2.0"
  String url;
  size_t size;
  uint8_t sha256[32];
};

// Splits "1.2.3" (an optional leading "v" is ignored); false for anything else, including "dev".
bool parseVersion(const char* text, int (&parts)[3]) {
  if (*text == 'v') text++;
  return sscanf(text, "%d.%d.%d", &parts[0], &parts[1], &parts[2]) == 3;
}

bool isNewer(const char* candidate) {
  int ours[3], theirs[3];
  if (!parseVersion(FIRMWARE_VERSION, ours) || !parseVersion(candidate, theirs)) return false;
  for (int i = 0; i < 3; i++) {
    if (theirs[i] != ours[i]) return theirs[i] > ours[i];
  }
  return false;
}

bool parseSha256(const char* hex, uint8_t (&out)[32]) {
  if (strlen(hex) != 64) return false;
  for (int i = 0; i < 32; i++) {
    if (sscanf(hex + 2 * i, "%2hhx", &out[i]) != 1) return false;
  }
  return true;
}

// Reads the latest release from GitHub's API, whose certificate the device can verify. The firmware
// itself is downloaded without verification (its host chains to ISRG Root X1, an RSA-4096 key the ESP32
// can't check mid-handshake for lack of heap) and is checked against the SHA-256 GitHub reports here.
bool fetchRelease(Release& release) {
  WiFiClientSecure tls;
  net::useTrustedRoots(tls);
  tls.setHandshakeTimeout(kTimeoutMs / 1000);
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding, so the JSON can be parsed straight off the stream
  http.setConnectTimeout(kTimeoutMs);
  http.setTimeout(kTimeoutMs);
  if (!http.begin(tls, kLatestUrl)) return false;
  http.addHeader("Accept", "application/vnd.github+json");
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("update: check failed (%d %s)\n", code, http.errorToString(code).c_str());
    http.end();
    return code > 0;  // an HTTP error (e.g. 404: no releases yet) still proves GitHub is reachable
  }

  JsonDocument filter;
  filter["tag_name"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["size"] = true;
  filter["assets"][0]["digest"] = true;
  filter["assets"][0]["browser_download_url"] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("update: bad release info (%s)\n", err.c_str());
    return false;
  }

  release.version = doc["tag_name"] | "";
  for (JsonObject asset : doc["assets"].as<JsonArray>()) {
    if (asset["name"] != kAssetName) continue;
    const char* digest = asset["digest"] | "";  // "sha256:<hex>"
    if (strncmp(digest, "sha256:", 7) != 0 || !parseSha256(digest + 7, release.sha256)) {
      Serial.println("update: release asset has no SHA-256 digest");
      break;
    }
    release.url = asset["browser_download_url"] | "";
    release.size = asset["size"] | 0;
  }
  Serial.printf("update: latest release is %s\n", release.version.c_str());
  return true;
}

// A newly installed version stays on probation until this runs; see verifyRollbackLater().
void confirmVersion() {
  static bool confirmed = false;
  if (confirmed) return;
  confirmed = true;
  if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) Serial.println("update: version confirmed");
}

// Streams the image into the idle slot, checking its SHA-256 on the way; restarts on success, so it
// only returns on failure.
void install(const Release& release) {
  snprintf(target, sizeof(target), "%s", release.version.c_str() + (release.version[0] == 'v'));
  progress = 0;
  is_installing = true;
  Serial.printf("update: installing %s (%u bytes)\n", target, (unsigned)release.size);

  WiFiClientSecure tls;
  tls.setInsecure();  // integrity comes from the SHA-256 check below
  tls.setHandshakeTimeout(kTimeoutMs / 1000);
  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // github.com -> release-assets host
  http.setConnectTimeout(kTimeoutMs);
  http.setTimeout(kTimeoutMs);
  if (!http.begin(tls, release.url) || http.GET() != HTTP_CODE_OK || http.getSize() != (int)release.size) {
    Serial.println("update: download failed");
    http.end();
    is_installing = false;
    return;
  }
  if (!Update.begin(release.size)) {
    Serial.printf("update: can't start (%s)\n", Update.errorString());
    http.end();
    is_installing = false;
    return;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1024];
  size_t written = 0;
  uint32_t last_data = millis();
  while (written < release.size && millis() - last_data < kTimeoutMs) {
    size_t n = stream->readBytes(buf, min(sizeof(buf), release.size - written));
    if (n == 0) continue;
    if (Update.write(buf, n) != n) break;
    mbedtls_sha256_update(&sha, buf, n);
    written += n;
    last_data = millis();
    progress = written * 100 / release.size;
  }
  http.end();
  uint8_t sha256[32];
  mbedtls_sha256_finish(&sha, sha256);
  mbedtls_sha256_free(&sha);

  bool intact = written == release.size && memcmp(sha256, release.sha256, sizeof(sha256)) == 0;
  if (intact && Update.end()) {
    Serial.println("update: installed, restarting");
    delay(500);
    ESP.restart();
  }
  Serial.printf("update: failed after %u bytes (%s)\n", (unsigned)written,
                intact ? Update.errorString() : "SHA-256 mismatch");
  Update.abort();
  is_installing = false;
}

// Checks once; returns false if it couldn't reach GitHub.
bool check() {
  // TLS needs ~40 KB of contiguous heap, which isn't there while the player's own session is open.
  if (!player::suspend()) return false;
  Serial.printf("update: checking (free heap %u, largest block %u)\n", (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());
  // The TLS handshake with api.github.com (ECDSA P-384, verified in software) computes for several
  // seconds without yielding, which would starve core 0's idle task into a watchdog reset.
  disableCore0WDT();
  Release release = {};
  bool reached = fetchRelease(release);
  enableCore0WDT();
  if (reached) {
    confirmVersion();
    if (release.size && isNewer(release.version.c_str())) install(release);
  }
  player::resume();
  return reached;
}

void task(void*) {
  uint32_t next_check_at = kFirstCheckMs;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    if (net::wifi() != net::Wifi::kConnected || !idle || (int32_t)(millis() - next_check_at) < 0) continue;
    next_check_at = millis() + (check() ? kCheckIntervalMs : kRetryMs);
  }
}

}  // namespace

void begin() {
  Serial.printf("update: running %s\n", FIRMWARE_VERSION);
  xTaskCreatePinnedToCore(task, "update", 8192, nullptr, 1, nullptr, 0);
}

void setIdle(bool is_idle) { idle = is_idle; }

bool installing() { return is_installing; }

uint8_t percent() { return progress; }

const char* targetVersion() { return target; }

const char* currentVersion() { return FIRMWARE_VERSION; }

}  // namespace updater
