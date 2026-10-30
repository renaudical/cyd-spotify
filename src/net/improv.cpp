#include "improv.h"

#include <WiFi.h>

#include "net.h"
#include "settings/settings.h"
#include "updater/updater.h"

namespace improv {
namespace {

// Packet: "IMPROV", version, type, data length, data, checksum (sum of all preceding bytes).
constexpr char kHeader[] = "IMPROV";
constexpr size_t kHeaderLen = 6;
constexpr uint8_t kVersion = 1;

enum Type : uint8_t { kCurrentState = 0x01, kErrorState = 0x02, kRpcCommand = 0x03, kRpcResult = 0x04 };
enum State : uint8_t { kReady = 0x02, kProvisioning = 0x03, kProvisioned = 0x04 };
enum Error : uint8_t { kNoError = 0x00, kInvalidRpc = 0x01, kUnknownRpc = 0x02, kUnableToConnect = 0x03 };
enum Command : uint8_t { kWifiSettings = 0x01, kGetState = 0x02, kGetInfo = 0x03, kScan = 0x04 };

uint8_t packet[256];
size_t received = 0;

// Written in one Serial.write so log lines from other tasks can't land in the middle.
void send(Type type, const uint8_t* data, uint8_t len) {
  uint8_t out[sizeof(packet) + 2];
  memcpy(out, kHeader, kHeaderLen);
  out[6] = kVersion;
  out[7] = type;
  out[8] = len;
  memcpy(out + 9, data, len);
  uint8_t sum = 0;
  for (size_t i = 0; i < 9u + len; i++) sum += out[i];
  out[9 + len] = sum;
  out[10 + len] = '\n';
  Serial.write(out, 11 + len);
}

void sendState(State state) {
  uint8_t b = state;
  send(kCurrentState, &b, 1);
}

void sendError(Error error) {
  uint8_t b = error;
  send(kErrorState, &b, 1);
}

// An RPC result is the command, the total length, then each string prefixed by its length.
void sendResult(Command command, std::initializer_list<String> strings) {
  uint8_t data[250];
  size_t len = 2;
  for (const String& s : strings) {
    size_t n = min(s.length(), sizeof(data) - len - 1);
    data[len++] = n;
    memcpy(data + len, s.c_str(), n);
    len += n;
  }
  data[0] = command;
  data[1] = len - 2;
  send(kRpcResult, data, len);
}

String url() { return "http://" + WiFi.localIP().toString() + "/"; }

void handleCommand(const uint8_t* data, uint8_t len, ConnectFn connect) {
  if (len < 2 || data[1] != len - 2) return sendError(kInvalidRpc);
  Command command = static_cast<Command>(data[0]);
  const uint8_t* args = data + 2;
  uint8_t args_len = data[1];

  switch (command) {
    case kWifiSettings: {
      uint8_t ssid_len = args_len ? args[0] : 0;
      if (args_len < ssid_len + 2u || args_len < ssid_len + 2u + args[1 + ssid_len]) return sendError(kInvalidRpc);
      String ssid(reinterpret_cast<const char*>(args + 1), ssid_len);
      String password(reinterpret_cast<const char*>(args + 2 + ssid_len), args[1 + ssid_len]);
      sendState(kProvisioning);
      if (!connect(ssid, password)) {
        sendError(kUnableToConnect);
        return sendState(kReady);
      }
      sendState(kProvisioned);
      return sendResult(kWifiSettings, {url()});
    }
    case kGetState:
      if (net::wifi() != net::Wifi::kConnected) return sendState(kReady);
      sendState(kProvisioned);
      return sendResult(kGetState, {url()});
    case kGetInfo:
      return sendResult(kGetInfo, {"Now Playing Brickman", updater::currentVersion(), "ESP32",
                                   String("Brickman ") + settings::deviceId()});
    case kScan: {
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n; i++) {
        sendResult(kScan, {WiFi.SSID(i), String(WiFi.RSSI(i)), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "NO" : "YES"});
      }
      WiFi.scanDelete();
      return sendResult(kScan, {});
    }
  }
  sendError(kUnknownRpc);
}

}  // namespace

void poll(ConnectFn connect) {
  while (Serial.available()) {
    uint8_t b = Serial.read();
    // Resynchronize on the header: anything that doesn't match it is dropped.
    if (received < kHeaderLen) {
      if (b == static_cast<uint8_t>(kHeader[received])) {
        packet[received++] = b;
      } else {
        received = b == static_cast<uint8_t>(kHeader[0]) ? 1 : 0;
        if (received) packet[0] = b;
      }
      continue;
    }
    packet[received++] = b;
    if (received < 9) continue;
    uint8_t len = packet[8];
    if (received < 10u + len) continue;

    received = 0;
    uint8_t sum = 0;
    for (size_t i = 0; i < 9u + len; i++) sum += packet[i];
    if (packet[6] != kVersion || sum != packet[9 + len]) {
      sendError(kInvalidRpc);
      continue;
    }
    if (packet[7] == kRpcCommand) handleCommand(packet + 9, len, connect);
  }
}

}  // namespace improv
