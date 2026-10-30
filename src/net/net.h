#pragma once

#include <Arduino.h>
#include <stdint.h>

class WiFiClientSecure;

// Wi-Fi link, kept alive by a FreeRTOS task on core 0 so the UI (core 1) never blocks on the network.
// With no network saved, or after failing to join it for a few minutes, the device also opens its own
// open network (setupNetworkName()) whose web page asks for one; see web/web_server.cpp.
namespace net {

enum class Wifi : uint8_t { kConnecting, kConnected, kLost };

void begin();

// Safe to call from any task.
Wifi wifi();

// True while the setup network is up.
bool inSetup();
// "Brickman-1A2B".
const char* setupNetworkName();
// The device's address on the user's network, e.g. "192.168.1.23"; empty when not connected.
String address();

// Makes the client verify servers against the embedded root CAs (data/cert/x509_crt_bundle.bin).
// Call before the first connection.
void useTrustedRoots(WiFiClientSecure& client);

}  // namespace net
