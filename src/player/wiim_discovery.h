#pragma once

#include <Arduino.h>

#include <vector>

// Finds WiiM (and other LinkPlay) streamers on the network: they advertise _linkplay._tcp over mDNS.
namespace wiim {

struct Device {
  String name;  // mDNS hostname, e.g. "WiiM-Mini-1A2B"
  String ip;
};

// Blocks for about two seconds. Needs Wi-Fi.
std::vector<Device> discover();

}  // namespace wiim
