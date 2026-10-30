#include "web_server.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "net/net.h"
#include "net/timezones.h"
#include "player/spotify.h"
#include "player/wiim_discovery.h"
#include "settings/settings.h"
#include "settings_page.h"
#include "updater/updater.h"

namespace web {
namespace {

constexpr uint32_t kRestartDelayMs = 1000;  // lets the response reach the browser first

WebServer server(80);
DNSServer dns;
bool dns_up = false;
uint32_t restart_at = 0;

void sendJson(const JsonDocument& doc) {
  String body;
  serializeJson(doc, body);
  server.send(200, "application/json", body);
}

void restartSoon() { restart_at = millis() | 1; }

void handleStatus() {
  JsonDocument doc;
  doc["name"] = String("Brickman ") + settings::deviceId();
  doc["version"] = updater::currentVersion();
  doc["setup"] = net::inSetup() && net::wifi() != net::Wifi::kConnected;
  doc["ssid"] = settings::wifiSsid();
  doc["ip"] = net::address();
  doc["hostname"] = String(settings::hostname()) + ".local";
  doc["source"] = settings::source() == settings::Source::kSpotify ? "spotify" : "wiim";
  doc["wiim_host"] = settings::wiimHost();
  doc["spotify_client_id"] = settings::spotifyClientId();
  doc["spotify_signed_in"] = spotify::signedIn();
  doc["spotify_device"] = settings::spotifyDevice();
  doc["spotify_redirect_uri"] = spotify::redirectUri();
  doc["timezone"] = settings::timezone();
  doc["timezone_detected"] = tz::detectedName();
  doc["flip_screen"] = settings::flipScreen();
  sendJson(doc);
}

void handleNetworks() {
  JsonDocument doc;
  JsonArray list = doc.to<JsonArray>();
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    bool seen = false;  // mesh networks list the same name once per access point
    for (JsonObject other : list) seen |= other["ssid"] == WiFi.SSID(i);
    if (seen || WiFi.SSID(i).isEmpty()) continue;
    JsonObject net = list.add<JsonObject>();
    net["ssid"] = WiFi.SSID(i);
    net["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  sendJson(doc);
}

void handleWiims() {
  JsonDocument doc;
  JsonArray list = doc.to<JsonArray>();
  if (net::wifi() == net::Wifi::kConnected) {
    for (const wiim::Device& d : wiim::discover()) {
      JsonObject o = list.add<JsonObject>();
      o["name"] = d.name;
      o["ip"] = d.ip;
    }
  }
  sendJson(doc);
}

// 500-odd names: streamed, so the whole list never sits in RAM.
void handleTimezones() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "[");
  String chunk;
  for (size_t i = 0; i < tz::kZoneCount; i++) {
    if (i) chunk += ',';
    chunk += '"';
    chunk += tz::kZones[i].name;
    chunk += '"';
    if (chunk.length() > 1024) {
      server.sendContent(chunk);
      chunk = "";
    }
  }
  chunk += ']';
  server.sendContent(chunk);
  server.sendContent("");
}

void handleSaveWifi() {
  String ssid = server.arg("ssid");
  if (ssid.isEmpty() || ssid.length() > 32 || server.arg("password").length() > 64) {
    server.send(400, "text/plain", "Invalid network name or password");
    return;
  }
  settings::setWifi(ssid, server.arg("password"));
  server.send(200, "text/plain", "ok");
  restartSoon();
}

void handleSaveSettings() {
  if (server.hasArg("source")) {
    settings::setSource(server.arg("source") == "spotify" ? settings::Source::kSpotify : settings::Source::kWiim);
  }
  if (server.hasArg("spotify_device")) settings::setSpotifyDevice(server.arg("spotify_device"));
  if (server.hasArg("wiim_host")) settings::setWiimHost(server.arg("wiim_host"));
  if (server.hasArg("timezone")) {
    String zone = server.arg("timezone");
    if (zone.length() && !tz::lookup(zone.c_str())) {
      server.send(400, "text/plain", "Unknown timezone");
      return;
    }
    settings::setTimezone(zone);
  }
  if (server.hasArg("flip_screen")) settings::setFlipScreen(server.arg("flip_screen") == "1");
  server.send(200, "text/plain", "ok");
  restartSoon();
}

// Saves the Client ID and answers with the Spotify sign-in URL for the browser to open.
void handleSpotifyLogin() {
  String client_id = server.arg("client_id");
  client_id.trim();
  if (client_id.length() != 32) {
    server.send(400, "text/plain", "A Client ID is 32 letters and digits; copy it from your Spotify app's settings");
    return;
  }
  JsonDocument doc;
  doc["url"] = spotify::beginLogin(client_id, net::address());
  sendJson(doc);
}

// Spotify -> GitHub Pages (site/spotify-callback.html) -> here, with ?code=&state= or ?error=.
void handleSpotifyCallback() {
  String error = server.arg("error");
  bool ok = false;
  if (error.isEmpty()) ok = spotify::finishLogin(server.arg("code"), server.arg("state"), error);
  if (error == "access_denied") error = "Spotify access wasn't allowed.";
  String page = "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
                "<title>Spotify</title><body style='font:16px system-ui;padding:32px;max-width:520px;margin:auto'>";
  if (ok) {
    page += "<h1>Spotify connected</h1><p>The display will show what you play. Taking you back to the settings&hellip;</p>"
            "<script>setTimeout(()=>location.href='/',2500)</script>";
  } else {
    page += "<h1>Couldn&rsquo;t connect Spotify</h1><p>";
    for (char c : error) page += c == '<' ? String("&lt;") : c == '&' ? String("&amp;") : String(c);
    page += "</p>";
  }
  page += "<p><a href='/'>Back to settings</a></p></body>";
  server.send(ok ? 200 : 400, "text/html", page);
}

void handleSpotifyDevices() {
  std::vector<String> names;
  String error;
  if (!spotify::devices(names, error)) {
    server.send(503, "text/plain", error);
    return;
  }
  JsonDocument doc;
  JsonArray list = doc.to<JsonArray>();
  for (const String& name : names) list.add(name);
  sendJson(doc);
}

void handleSpotifySignOut() {
  spotify::signOut();
  server.send(200, "text/plain", "ok");
  restartSoon();
}

void handleRestart() {
  server.send(200, "text/plain", "ok");
  restartSoon();
}

void handlePage() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", kSettingsPage);
}

// On the setup network every unknown address leads to the page, which is what makes phones pop it up.
void handleNotFound() {
  if (net::inSetup()) {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "Not found");
}

void task(void*) {
  while (WiFi.getMode() == WIFI_OFF) vTaskDelay(pdMS_TO_TICKS(100));  // the net task brings up the stack
  server.on("/", HTTP_GET, handlePage);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/networks", HTTP_GET, handleNetworks);
  server.on("/api/wiims", HTTP_GET, handleWiims);
  server.on("/api/timezones", HTTP_GET, handleTimezones);
  server.on("/api/wifi", HTTP_POST, handleSaveWifi);
  server.on("/api/settings", HTTP_POST, handleSaveSettings);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.on("/api/spotify/login", HTTP_POST, handleSpotifyLogin);
  server.on("/api/spotify/devices", HTTP_GET, handleSpotifyDevices);
  server.on("/api/spotify/signout", HTTP_POST, handleSpotifySignOut);
  server.on("/spotify/callback", HTTP_GET, handleSpotifyCallback);
  server.onNotFound(handleNotFound);
  server.begin();

  for (;;) {
    // The captive DNS (every name resolves to us) only runs while the setup network is up.
    if (net::inSetup() != dns_up) {
      if (net::inSetup()) {
        dns.start(53, "*", WiFi.softAPIP());
      } else {
        dns.stop();
      }
      dns_up = net::inSetup();
    }
    if (dns_up) dns.processNextRequest();
    server.handleClient();
    if (restart_at && millis() - restart_at > kRestartDelayMs) ESP.restart();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

}  // namespace

void begin() { xTaskCreatePinnedToCore(task, "web", 8192, nullptr, 1, nullptr, 0); }

}  // namespace web
