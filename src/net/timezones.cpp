#include "timezones.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClient.h>
#include <string.h>
#include <time.h>

#include <atomic>

#include "settings/settings.h"

namespace tz {
namespace {

#define NTP_SERVERS "pool.ntp.org", "time.google.com"

// ip-api.com: free, no key, and plain HTTP, so it doesn't need a TLS session's worth of heap.
constexpr char kLookupUrl[] = "http://ip-api.com/json/?fields=status,timezone";
constexpr uint32_t kTimeoutMs = 5000;
constexpr uint32_t kFirstRetryMs = 30 * 1000;
constexpr uint32_t kMaxRetryMs = 30 * 60 * 1000;

constexpr char kPrefsNamespace[] = "now-playing";  // shared with the theme choice
constexpr char kPrefsKey[] = "tz";

bool started = false;
bool done = false;  // detected, or nothing left to detect
uint32_t next_attempt_at = 0;
uint32_t retry_ms = kFirstRetryMs;
std::atomic<bool> is_known{false};
char detected_name[48] = "";  // IANA name from this boot's lookup; set before is_known

// Asks ip-api.com which zone this network's public IP is in. A VPN gives the VPN's zone.
bool detect(String& iana) {
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(kTimeoutMs);
  http.setTimeout(kTimeoutMs);
  if (!http.begin(client, kLookupUrl)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("tz: lookup failed (%d %s)\n", code, http.errorToString(code).c_str());
    http.end();
    return false;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getString());
  http.end();
  if (err || doc["status"] != "success") {
    Serial.println("tz: lookup gave no zone");
    return false;
  }
  iana = doc["timezone"] | "";
  return iana.length() > 0;
}

void detectAndApply() {
  String iana;
  if (!detect(iana)) {
    next_attempt_at = millis() + retry_ms;
    retry_ms = min(retry_ms * 2, kMaxRetryMs);
    return;
  }
  done = true;
  const char* posix = lookup(iana.c_str());
  if (!posix) {
    Serial.printf("tz: '%s' isn't in the table (rerun tools/gen_timezones.py); set it in settings\n", iana.c_str());
    return;
  }
  Serial.printf("tz: detected %s (%s)\n", iana.c_str(), posix);
  snprintf(detected_name, sizeof(detected_name), "%s", iana.c_str());
  setenv("TZ", posix, 1);
  tzset();
  is_known = true;

  Preferences prefs;
  prefs.begin(kPrefsNamespace);
  if (!prefs.isKey(kPrefsKey) || prefs.getString(kPrefsKey) != posix) prefs.putString(kPrefsKey, posix);
  prefs.end();
}

}  // namespace

const char* lookup(const char* iana) {
  size_t lo = 0, hi = kZoneCount;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    int cmp = strcmp(iana, kZones[mid].name);
    if (cmp == 0) return kZones[mid].posix;
    if (cmp < 0) hi = mid;
    else lo = mid + 1;
  }
  return nullptr;
}

bool known() { return is_known; }

const char* detectedName() { return is_known ? detected_name : ""; }

void poll() {
  if (!started) {
    started = true;
    String manual = settings::timezone();
    if (manual.length()) {
      // The settings page saves an IANA name; secrets.h's TIMEZONE seeds a POSIX string.
      const char* posix = lookup(manual.c_str());
      configTzTime(posix ? posix : manual.c_str(), NTP_SERVERS);
      Serial.printf("tz: %s (set in settings)\n", manual.c_str());
      is_known = true;
      done = true;
      return;
    }
    // Use the last detected zone right away, then check it once per boot in case the device moved.
    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    String saved = prefs.isKey(kPrefsKey) ? prefs.getString(kPrefsKey) : "";
    prefs.end();
    configTzTime(saved.length() ? saved.c_str() : "UTC0", NTP_SERVERS);
    is_known = saved.length() > 0;
  }
  if (!done && (int32_t)(millis() - next_attempt_at) >= 0) detectAndApply();
}

}  // namespace tz
