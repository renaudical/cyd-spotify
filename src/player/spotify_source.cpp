// Spotify source: polls the Web API's playback state for the signed-in account, optionally only for one
// Spotify Connect device. Needs Spotify Premium (Spotify requires it of developer apps' owners).
#ifndef FAKE_PLAYER

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_random.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>

#include <atomic>

#include "net/net.h"
#include "settings/settings.h"
#include "source.h"
#include "spotify.h"

#ifndef SPOTIFY_REDIRECT_URI
#define SPOTIFY_REDIRECT_URI "https://renaudical.github.io/cyd-spotify/spotify-callback.html"
#endif

namespace {

constexpr char kAuthorizeUrl[] = "https://accounts.spotify.com/authorize";
constexpr char kTokenUrl[] = "https://accounts.spotify.com/api/token";
constexpr char kApiUrl[] = "https://api.spotify.com/v1";
constexpr char kScopes[] = "user-read-playback-state user-modify-playback-state user-read-currently-playing";
constexpr uint16_t kTimeoutMs = 8000;
// Polling: the position is interpolated between polls, so these only bound how late a change shows.
constexpr uint32_t kPlayingPollMs = 2000;
constexpr uint32_t kIdlePollMs = 5000;
constexpr uint32_t kExchangeTimeoutMs = 20000;
constexpr uint32_t kDevicesTimeoutMs = 15000;

// Work the web task hands to the player task, one request at a time (jobLock()).
enum class Job : uint8_t { kNone, kExchangeCode, kListDevices };
std::atomic<Job> job{Job::kNone};
std::atomic<bool> job_done{false};
std::atomic<bool> running{false};  // the Spotify source is the one polling
bool job_ok = false;
String job_error;
String job_code;
std::vector<String> job_devices;
// From beginLogin(), checked and used by finishLogin().
String login_verifier;
String login_nonce;

SemaphoreHandle_t jobLock() {
  static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
  return lock;
}

String randomString(size_t n) {
  static const char kChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
  String s;
  for (size_t i = 0; i < n; i++) s += kChars[esp_random() % (sizeof(kChars) - 1)];
  return s;
}

String base64url(const uint8_t* data, size_t len) {
  unsigned char out[64];
  size_t n = 0;
  mbedtls_base64_encode(out, sizeof(out), &n, data, len);
  String s;
  for (size_t i = 0; i < n; i++) {
    char c = out[i];
    if (c == '=') break;
    s += c == '+' ? '-' : c == '/' ? '_' : c;
  }
  return s;
}

String urlEncode(const String& s) {
  String out;
  for (char c : s) {
    if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '.' || c == '_' || c == '~') {
      out += c;
    } else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02X", static_cast<uint8_t>(c));
      out += hex;
    }
  }
  return out;
}

// Hands a job to the player task and waits for it.
bool runJob(Job j, uint32_t timeout_ms) {
  if (!running) {
    job_error = "Choose Spotify as the music source first";
    return false;
  }
  job_ok = false;
  job_error = "";
  job_done = false;
  job = j;
  player::wake();
  uint32_t started = millis();
  while (!job_done) {
    if (millis() - started > timeout_ms) {
      job = Job::kNone;
      job_error = "The display took too long to answer";
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  job = Job::kNone;
  return job_ok;
}

}  // namespace

namespace player {
namespace {

class SpotifySource : public Source {
 public:
  const char* name() const override { return "Spotify"; }

  Result poll(Poll& out) override {
    if (!started_) start();
    if (refresh_token_.isEmpty()) return Result::kNeedsSetup;
    if ((int32_t)(millis() - retry_at_) < 0) return Result::kFailed;  // rate limited
    if (!ensureToken()) return refresh_token_.isEmpty() ? Result::kNeedsSetup : Result::kFailed;

    int code;
    String body;
    if (!api("GET", "/me/player?additional_types=episode&market=from_token", code, &body)) return Result::kFailed;
    if (code == 401) {  // expired early; get a new token and try once more
      access_token_ = "";
      if (!ensureToken()) return refresh_token_.isEmpty() ? Result::kNeedsSetup : Result::kFailed;
      if (!api("GET", "/me/player?additional_types=episode&market=from_token", code, &body)) return Result::kFailed;
    }
    if (code == 204) return nothingPlaying(out);  // no active device
    if (code != 200) {
      Serial.printf("spotify: player request failed (%d)\n", code);
      return Result::kFailed;
    }
    return parsePlayer(body, out);
  }

  String artUrl() override { return art_url_; }

  void send(Command command, const Poll& last) override {
    if (filtered_out_) return;  // what's playing is on another device; don't touch it
    const char* method = "POST";
    const char* path = command == Command::kNext ? "/me/player/next" : "/me/player/previous";
    if (command == Command::kTogglePlayPause) {
      method = "PUT";
      path = last.status == Status::kPlaying ? "/me/player/pause" : "/me/player/play";
    }
    int code;
    if (!ensureToken() || !api(method, path, code, nullptr)) return;
    if (code == 403) Serial.println("spotify: controls need Spotify Premium");
    else if (code >= 300) Serial.printf("spotify: %s %s failed (%d)\n", method, path, code);
  }

  void disconnect() override {
    http_.end();
    tls_.stop();
  }

  uint32_t pollIntervalMs(Status status) const override {
    return status == Status::kPlaying ? kPlayingPollMs : kIdlePollMs;
  }

  void service() override {
    if (!started_) start();
    Job j = job;
    if (j == Job::kNone || job_done) return;
    String error;
    bool ok = false;
    if (j == Job::kExchangeCode) {
      ok = exchangeCode(job_code, error);
    } else if (j == Job::kListDevices) {
      ok = listDevices(job_devices, error);
    }
    job_error = error;
    job_ok = ok;
    job_done = true;
  }

 private:
  void start() {
    started_ = true;
    running = true;
    refresh_token_ = settings::spotifyRefreshToken();
    device_filter_ = settings::spotifyDevice();
    net::useTrustedRoots(tls_);
    tls_.setHandshakeTimeout(kTimeoutMs / 1000);
    http_.setReuse(true);
    http_.setConnectTimeout(kTimeoutMs);
    http_.setTimeout(kTimeoutMs);
    static const char* kHeaders[] = {"Retry-After"};
    http_.collectHeaders(kHeaders, 1);
  }

  // One Web API request on the kept-alive connection. body is only read for 200 responses.
  bool api(const char* method, const String& path, int& code, String* body) {
    if (!http_.begin(tls_, String(kApiUrl) + path)) return false;
    http_.addHeader("Authorization", "Bearer " + access_token_);
    if (strcmp(method, "GET") != 0) http_.addHeader("Content-Length", "0");  // Spotify rejects bodiless PUTs without it
    code = http_.sendRequest(method, static_cast<uint8_t*>(nullptr), 0);
    if (code < 0) {
      Serial.printf("spotify: %s failed (%s)\n", path.c_str(), http_.errorToString(code).c_str());
      http_.end();
      return false;
    }
    if (code == 429) {
      uint32_t seconds = max(1L, http_.header("Retry-After").toInt());
      Serial.printf("spotify: rate limited for %u s\n", (unsigned)seconds);
      retry_at_ = millis() + seconds * 1000;
    }
    String text = http_.getString();  // read it all so the connection can be reused
    if (body && code == 200) *body = text;
    http_.end();
    return true;
  }

  // Valid access token, refreshing it if needed. On a rejected refresh token, signs out.
  bool ensureToken() {
    if (access_token_.length() && (int32_t)(millis() - token_expires_at_) < 0) return true;
    String error;
    String form = "grant_type=refresh_token&refresh_token=" + urlEncode(refresh_token_) +
                  "&client_id=" + urlEncode(settings::spotifyClientId());
    if (tokenRequest(form, error)) return true;
    Serial.printf("spotify: token refresh failed (%s)\n", error.c_str());
    return false;
  }

  bool exchangeCode(const String& code, String& error) {
    String form = "grant_type=authorization_code&code=" + urlEncode(code) +
                  "&redirect_uri=" + urlEncode(SPOTIFY_REDIRECT_URI) +
                  "&client_id=" + urlEncode(settings::spotifyClientId()) + "&code_verifier=" + login_verifier;
    if (!tokenRequest(form, error)) return false;
    Serial.println("spotify: signed in");
    return true;
  }

  // POSTs to the token endpoint. Its own connection, after closing the API one: two don't fit in RAM.
  bool tokenRequest(const String& form, String& error) {
    disconnect();
    WiFiClientSecure tls;
    net::useTrustedRoots(tls);
    tls.setHandshakeTimeout(kTimeoutMs / 1000);
    HTTPClient http;
    http.setConnectTimeout(kTimeoutMs);
    http.setTimeout(kTimeoutMs);
    if (!http.begin(tls, kTokenUrl)) {
      error = "Couldn't reach Spotify";
      return false;
    }
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int code = http.POST(form);
    String body = code > 0 ? http.getString() : "";
    http.end();
    if (code < 0) {
      error = "Couldn't reach Spotify";
      return false;
    }

    JsonDocument doc;
    deserializeJson(doc, body);
    if (code != 200) {
      error = doc["error_description"] | "";
      if (error.isEmpty()) error = doc["error"] | String(code);
      if (doc["error"] == "invalid_grant" || doc["error"] == "invalid_client") {
        // The refresh token was revoked or the app changed: sign in again.
        refresh_token_ = "";
        settings::setSpotifyRefreshToken("");
      }
      return false;
    }
    access_token_ = doc["access_token"] | "";
    token_expires_at_ = millis() + ((doc["expires_in"] | 3600) - 60) * 1000UL;
    String refresh = doc["refresh_token"] | "";
    if (refresh.length() && refresh != refresh_token_) {  // Spotify may rotate it
      refresh_token_ = refresh;
      settings::setSpotifyRefreshToken(refresh);
    }
    return access_token_.length() > 0;
  }

  bool listDevices(std::vector<String>& names, String& error) {
    names.clear();
    int code;
    String body;
    if (!ensureToken()) {
      error = "Not signed in to Spotify";
      return false;
    }
    if (!api("GET", "/me/player/devices", code, &body) || code != 200) {
      error = "Couldn't get the device list from Spotify";
      return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, body)) {
      error = "Spotify sent an unreadable device list";
      return false;
    }
    for (JsonObject d : doc["devices"].as<JsonArray>()) names.push_back(d["name"] | "");
    return true;
  }

  Result nothingPlaying(Poll& out) {
    out = {};
    out.status = Status::kStopped;
    out.polled_at_ms = millis();
    art_url_ = "";
    return Result::kOk;
  }

  // The smallest image at least as big as the art box (Spotify sends 640, 300 and 64 px).
  static String pickImage(JsonArrayConst images) {
    String best;
    int best_width = 0;
    for (JsonObjectConst image : images) {
      int width = image["width"] | 0;
      bool better = best.isEmpty() || (width >= 100 && (best_width < 100 || width < best_width)) ||
                    (best_width < 100 && width > best_width);
      if (better) {
        best = image["url"] | "";
        best_width = width;
      }
    }
    return best;
  }

  Result parsePlayer(const String& body, Poll& out) {
    JsonDocument filter;
    filter["is_playing"] = true;
    filter["progress_ms"] = true;
    filter["currently_playing_type"] = true;
    filter["device"]["name"] = true;
    JsonObject item = filter["item"].to<JsonObject>();
    item["name"] = true;
    item["duration_ms"] = true;
    item["artists"][0]["name"] = true;
    item["album"]["name"] = true;
    item["album"]["images"][0]["url"] = true;
    item["album"]["images"][0]["width"] = true;
    item["show"]["name"] = true;  // podcast episodes
    item["images"][0]["url"] = true;
    item["images"][0]["width"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (err) {
      Serial.printf("spotify: bad player reply (%s)\n", err.c_str());
      return Result::kFailed;
    }

    filtered_out_ = device_filter_.length() && device_filter_ != (doc["device"]["name"] | "");
    if (filtered_out_) return nothingPlaying(out);

    out.status = doc["is_playing"] ? Status::kPlaying : Status::kPaused;
    out.position_ms = doc["progress_ms"] | 0;
    out.polled_at_ms = millis();
    JsonObjectConst track = doc["item"];
    if (track.isNull()) {  // ads, and moments between tracks
      copyText(out.title, sizeof(out.title), doc["currently_playing_type"] == "ad" ? "Advertisement" : "");
      out.artist[0] = out.album[0] = '\0';
      out.duration_ms = 0;
      art_url_ = "";
      return Result::kOk;
    }

    copyText(out.title, sizeof(out.title), track["name"] | "");
    out.duration_ms = track["duration_ms"] | 0;
    if (track["show"].is<JsonObjectConst>()) {
      copyText(out.artist, sizeof(out.artist), track["show"]["name"] | "");
      out.album[0] = '\0';
      art_url_ = pickImage(track["images"].as<JsonArrayConst>());
    } else {
      String artists;
      for (JsonObjectConst a : track["artists"].as<JsonArrayConst>()) {
        if (artists.length()) artists += ", ";
        artists += a["name"] | "";
      }
      copyText(out.artist, sizeof(out.artist), artists.c_str());
      copyText(out.album, sizeof(out.album), track["album"]["name"] | "");
      art_url_ = pickImage(track["album"]["images"].as<JsonArrayConst>());
    }
    return Result::kOk;
  }

  bool started_ = false;
  String refresh_token_;
  String access_token_;
  uint32_t token_expires_at_ = 0;
  uint32_t retry_at_ = 0;
  String device_filter_;
  bool filtered_out_ = false;
  String art_url_;
  WiFiClientSecure tls_;
  HTTPClient http_;
};

}  // namespace

Source* spotifySource() {
  static SpotifySource source;
  return &source;
}

}  // namespace player

namespace spotify {

const char* redirectUri() { return SPOTIFY_REDIRECT_URI; }

String beginLogin(const String& client_id, const String& address) {
  xSemaphoreTake(jobLock(), portMAX_DELAY);
  settings::setSpotifyClientId(client_id);
  login_verifier = randomString(64);
  login_nonce = randomString(16);
  uint8_t hash[32];
  mbedtls_sha256(reinterpret_cast<const uint8_t*>(login_verifier.c_str()), login_verifier.length(), hash, 0);
  String url = String(kAuthorizeUrl) + "?response_type=code&client_id=" + urlEncode(client_id) +
               "&scope=" + urlEncode(kScopes) + "&redirect_uri=" + urlEncode(SPOTIFY_REDIRECT_URI) +
               "&code_challenge_method=S256&code_challenge=" + base64url(hash, sizeof(hash)) +
               "&state=" + urlEncode(login_nonce + "~" + address);
  xSemaphoreGive(jobLock());
  return url;
}

bool finishLogin(const String& code, const String& state, String& error) {
  xSemaphoreTake(jobLock(), portMAX_DELAY);
  bool ok = false;
  if (login_nonce.isEmpty() || !state.startsWith(login_nonce + "~")) {
    error = "This sign-in didn't start on this display, or the display restarted. Try again.";
  } else {
    job_code = code;
    ok = runJob(Job::kExchangeCode, kExchangeTimeoutMs);
    error = job_error;
    login_nonce = "";  // single use
  }
  xSemaphoreGive(jobLock());
  return ok;
}

bool signedIn() { return settings::spotifyRefreshToken().length() > 0; }

void signOut() { settings::setSpotifyRefreshToken(""); }

bool devices(std::vector<String>& names, String& error) {
  xSemaphoreTake(jobLock(), portMAX_DELAY);
  bool ok = runJob(Job::kListDevices, kDevicesTimeoutMs);
  names = job_devices;
  error = job_error;
  xSemaphoreGive(jobLock());
  return ok;
}

}  // namespace spotify

#endif  // FAKE_PLAYER
