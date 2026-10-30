#include "net.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <atomic>

#include "improv.h"
#include "settings/settings.h"
#include "timezones.h"

namespace net {
namespace {

constexpr uint32_t kPollMs = 100;  // also how often Improv packets from the installer are read
// Give up on a connection attempt and start a fresh one after this long.
constexpr uint32_t kAttemptTimeoutMs = 15000;
// Open the setup network after failing to join the saved one for this long (the password may have
// changed, or the device moved). Joining keeps being retried, less often, while it's up.
constexpr uint32_t kSetupAfterMs = 3 * 60 * 1000;
constexpr uint32_t kSetupRetryMs = 60000;
// How long a network from the installer gets to connect before it's reported as wrong.
constexpr uint32_t kProvisionTimeoutMs = 20000;

std::atomic<Wifi> state{Wifi::kConnecting};
std::atomic<bool> setup_up{false};
char setup_name[20];
String ssid;
String password;

void startSetup() {
  if (setup_up) return;
  Serial.printf("wifi: opening setup network %s\n", setup_name);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(setup_name);
  setup_up = true;
}

void stopSetup() {
  if (!setup_up) return;
  Serial.println("wifi: closing setup network");
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  setup_up = false;
}

void onConnected() {
  Serial.printf("wifi: connected to %s, ip %s, rssi %d dBm\n", ssid.c_str(), WiFi.localIP().toString().c_str(),
                WiFi.RSSI());
  state = Wifi::kConnected;
  stopSetup();
  MDNS.end();
  if (MDNS.begin(settings::hostname())) MDNS.addService("http", "tcp", 80);
}

// From the installer, over USB (Improv).
bool provision(const String& new_ssid, const String& new_password) {
  Serial.printf("wifi: trying %s from the installer\n", new_ssid.c_str());
  WiFi.disconnect();
  WiFi.begin(new_ssid.c_str(), new_password.c_str());
  uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - started > kProvisionTimeoutMs) {
      Serial.println("wifi: couldn't join it");
      WiFi.disconnect();
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  settings::setWifi(new_ssid, new_password);
  ssid = new_ssid;
  password = new_password;
  onConnected();
  return true;
}

void task(void*) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(settings::hostname());
  WiFi.setSleep(false);  // power save adds 100+ ms latency to every request
  ssid = settings::wifiSsid();
  password = settings::wifiPassword();
  if (ssid.isEmpty()) startSetup();

  uint32_t attempt_started = 0;
  uint32_t trying_since = millis();
  for (;;) {
    improv::poll(provision);

    if (WiFi.status() == WL_CONNECTED) {
      if (state != Wifi::kConnected) onConnected();
      tz::poll();
    } else {
      if (state == Wifi::kConnected) {
        Serial.println("wifi: connection lost");
        state = Wifi::kLost;
        attempt_started = 0;
        trying_since = millis();
      }
      if (!ssid.isEmpty()) {
        if (millis() - trying_since > kSetupAfterMs) startSetup();
        uint32_t retry_ms = setup_up ? kSetupRetryMs : kAttemptTimeoutMs;
        if (attempt_started == 0 || millis() - attempt_started > retry_ms) {
          Serial.printf("wifi: connecting to %s\n", ssid.c_str());
          WiFi.disconnect();
          WiFi.begin(ssid.c_str(), password.c_str());
          attempt_started = millis() | 1;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(kPollMs));
  }
}

}  // namespace

void begin() {
  snprintf(setup_name, sizeof(setup_name), "Brickman-%s", settings::deviceId());
  for (char* c = setup_name + 9; *c; c++) *c = toupper(*c);
  xTaskCreatePinnedToCore(task, "net", 8192, nullptr, 1, nullptr, 0);
}

Wifi wifi() { return state; }

bool inSetup() { return setup_up; }

const char* setupNetworkName() { return setup_name; }

String address() { return state == Wifi::kConnected ? WiFi.localIP().toString() : String(); }

// Embedded by board_build.embed_files in platformio.ini.
extern const uint8_t x509_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

void useTrustedRoots(WiFiClientSecure& client) { client.setCACertBundle(x509_crt_bundle_start); }

}  // namespace net
