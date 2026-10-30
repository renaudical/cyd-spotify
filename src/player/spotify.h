#pragma once

#include <Arduino.h>

#include <vector>

// Spotify sign-in and device list, for the settings page (web/web_server.cpp). Signing in uses the
// Authorization Code flow with PKCE against the user's own Spotify developer app, so there's no client
// secret. Spotify only redirects to HTTPS addresses, so it sends the browser to redirectUri() on GitHub
// Pages (site/spotify-callback.html), which forwards it to this device's /spotify/callback; the device's
// local address travels in the OAuth state.
//
// The network work runs on the player task (only one TLS session fits in RAM), so finishLogin() and
// devices() block the calling task and only work while Spotify is the selected source.
namespace spotify {

// What to enter as the Redirect URI in the Spotify app's settings.
const char* redirectUri();

// Saves the Client ID and returns the accounts.spotify.com URL to send the browser to. `address` is how
// the browser reaches this device, e.g. "192.168.1.23".
String beginLogin(const String& client_id, const String& address);

// Exchanges the code from Spotify's redirect for tokens. Blocks for up to ~20 s.
bool finishLogin(const String& code, const String& state, String& error);

bool signedIn();
void signOut();

// The account's Spotify Connect devices, by name. Blocks for up to ~15 s.
bool devices(std::vector<String>& names, String& error);

}  // namespace spotify
