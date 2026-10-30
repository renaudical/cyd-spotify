#pragma once

#include <Arduino.h>

// Improv Wi-Fi over the USB serial port (https://www.improv-wifi.com/serial/): right after flashing, the
// web installer asks for the Wi-Fi network and sends it here, then offers a link to the settings page.
// Runs on the net task, which owns Wi-Fi.
namespace improv {

// Tries to join the network and saves it on success; blocks until connected or given up.
using ConnectFn = bool (*)(const String& ssid, const String& password);

// Reads whatever has arrived on Serial and answers any complete Improv packets.
void poll(ConnectFn connect);

}  // namespace improv
