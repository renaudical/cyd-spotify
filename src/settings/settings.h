#pragma once

#include <Arduino.h>

// User settings, kept in NVS (namespace "now-playing") and edited on the device's web page or, for
// Wi-Fi, by the web installer over USB. include/secrets.h is optional: if present, its values seed
// settings that are still empty on first boot, which keeps local development quick.
// Safe to call from any task; changes that need a restart (Wi-Fi, music source) are applied by one.
namespace settings {

void begin();

String wifiSsid();
String wifiPassword();
void setWifi(const String& ssid, const String& password);

// The WiiM's address; empty means find it on the network.
String wiimHost();
void setWiimHost(const String& host);

// POSIX TZ string; empty means detect it from the public IP address.
String timezone();
void setTimezone(const String& posix);

// Short id from the MAC address, e.g. "1a2b": names the setup network ("Brickman-1A2B") and the
// mDNS hostname ("brickman-1a2b.local") so several displays on one network don't collide.
const char* deviceId();
const char* hostname();

}  // namespace settings
