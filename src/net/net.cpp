#include "net.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <atomic>

#if !__has_include("secrets.h")
#error "Missing include/secrets.h: copy include/secrets.example.h to include/secrets.h and fill it in."
#endif
#include "secrets.h"
#include "timezones.h"

namespace net {
namespace {

constexpr uint32_t kPollMs = 250;
// Give up on a connection attempt and start a fresh one after this long.
constexpr uint32_t kAttemptTimeoutMs = 15000;

std::atomic<Wifi> state{Wifi::kConnecting};

void task(void*) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("now-playing-brickman");
  WiFi.setSleep(false);  // power save adds 100+ ms latency to every request

  uint32_t attempt_started = 0;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (state != Wifi::kConnected) {
        Serial.printf("wifi: connected, ip %s, rssi %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        state = Wifi::kConnected;
      }
      tz::poll();
    } else {
      if (state == Wifi::kConnected) {
        Serial.println("wifi: connection lost");
        state = Wifi::kLost;
        attempt_started = 0;
      }
      if (attempt_started == 0 || millis() - attempt_started > kAttemptTimeoutMs) {
        Serial.println("wifi: connecting");
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        attempt_started = millis() | 1;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(kPollMs));
  }
}

}  // namespace

void begin() { xTaskCreatePinnedToCore(task, "net", 8192, nullptr, 1, nullptr, 0); }

Wifi wifi() { return state; }

// Embedded by board_build.embed_files in platformio.ini.
extern const uint8_t x509_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

void useTrustedRoots(WiFiClientSecure& client) { client.setCACertBundle(x509_crt_bundle_start); }

}  // namespace net
