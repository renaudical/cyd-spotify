#include "settings.h"

#include <Preferences.h>
#include <esp_mac.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

namespace settings {
namespace {

constexpr char kNamespace[] = "now-playing";  // shared with the theme choice and detected timezone
constexpr char kWifiSsid[] = "wifi_ssid";
constexpr char kWifiPassword[] = "wifi_pass";
constexpr char kWiimHost[] = "wiim_host";
constexpr char kTimezone[] = "tz_manual";

char device_id[5];
char host_name[24];

String get(const char* key) {
  Preferences prefs;
  prefs.begin(kNamespace, true);
  String value = prefs.isKey(key) ? prefs.getString(key) : "";
  prefs.end();
  return value;
}

void put(const char* key, const String& value) {
  Preferences prefs;
  prefs.begin(kNamespace);
  prefs.putString(key, value);
  prefs.end();
}

void seed(const char* key, const char* value) {
  if (*value && get(key).isEmpty()) put(key, value);
}

}  // namespace

void begin() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(device_id, sizeof(device_id), "%02x%02x", mac[4], mac[5]);
  snprintf(host_name, sizeof(host_name), "brickman-%s", device_id);

#ifdef WIFI_SSID
  seed(kWifiSsid, WIFI_SSID);
  seed(kWifiPassword, WIFI_PASSWORD);
#endif
#ifdef WIIM_HOST
  seed(kWiimHost, WIIM_HOST);
#endif
#ifdef TIMEZONE
  seed(kTimezone, TIMEZONE);
#endif
}

String wifiSsid() { return get(kWifiSsid); }
String wifiPassword() { return get(kWifiPassword); }

void setWifi(const String& ssid, const String& password) {
  put(kWifiSsid, ssid);
  put(kWifiPassword, password);
}

String wiimHost() { return get(kWiimHost); }
void setWiimHost(const String& host) { put(kWiimHost, host); }

String timezone() { return get(kTimezone); }
void setTimezone(const String& posix) { put(kTimezone, posix); }

const char* deviceId() { return device_id; }
const char* hostname() { return host_name; }

}  // namespace settings
