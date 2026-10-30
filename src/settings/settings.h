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

// Where "now playing" comes from. Defaults to the WiiM if one was configured (dev boards seeded from
// secrets.h), otherwise Spotify.
enum class Source { kWiim, kSpotify };
Source source();
void setSource(Source source);

// Spotify sign-in: the Client ID of the user's own Spotify developer app, and the refresh token from
// signing in (empty when signed out).
String spotifyClientId();
void setSpotifyClientId(const String& id);
String spotifyRefreshToken();
void setSpotifyRefreshToken(const String& token);
// Only show what plays on the Spotify Connect device with this name; empty means any device.
String spotifyDevice();
void setSpotifyDevice(const String& name);

// The WiiM's address; empty means find it on the network.
String wiimHost();
void setWiimHost(const String& host);

// POSIX TZ string; empty means detect it from the public IP address.
String timezone();
void setTimezone(const String& posix);

// Draw the screen rotated 180°. On by default: the 3D-printed case mounts the board with its USB ports
// on the other side. Off is for a bare board. Safe to read before begin().
bool flipScreen();
void setFlipScreen(bool flip);

// Short id from the MAC address, e.g. "1a2b": names the setup network ("Brickman-1A2B") and the
// mDNS hostname ("brickman-1a2b.local") so several displays on one network don't collide.
const char* deviceId();
const char* hostname();

}  // namespace settings
