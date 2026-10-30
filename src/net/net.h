#pragma once

#include <stdint.h>

// Wi-Fi link, kept alive by a FreeRTOS task on core 0 so the UI (core 1) never blocks on the network.
namespace net {

enum class Wifi : uint8_t { kConnecting, kConnected, kLost };

void begin();

// Safe to call from any task.
Wifi wifi();

}  // namespace net
