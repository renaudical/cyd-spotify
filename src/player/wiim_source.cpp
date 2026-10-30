// WiiM (LinkPlay) source: polls the streamer's HTTPS API on the local network. Shows whatever that
// streamer plays, from any app (Spotify Connect, AirPlay, radio...).
#ifndef FAKE_PLAYER

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "settings/settings.h"
#include "source.h"
#include "wiim_discovery.h"
#include "wiim_text.h"

namespace player {
namespace {

constexpr uint32_t kPollMs = 1000;
constexpr uint16_t kTimeoutMs = 3000;
// Failed polls in a row before looking for the WiiM again (it may have a new address).
constexpr int kFailuresBeforeRediscover = 3;

class WiimSource : public Source {
 public:
  const char* name() const override { return "WiiM"; }

  Result poll(Poll& out) override {
    if (!started_) start();
    if (host_.isEmpty() && !findHost()) return Result::kNotFound;

    String reply;
    if (!request("getPlayerStatus", reply)) return failed();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, reply);
    if (err) {
      Serial.printf("wiim: bad getPlayerStatus reply (%s)\n", err.c_str());
      return failed();
    }
    failures_ = 0;
    out.status = parseStatus(doc["status"] | "");
    wiim_text::decode(doc["Title"] | "", out.title, sizeof(out.title));
    wiim_text::decode(doc["Artist"] | "", out.artist, sizeof(out.artist));
    wiim_text::decode(doc["Album"] | "", out.album, sizeof(out.album));
    out.position_ms = strtoul(doc["curpos"] | "0", nullptr, 10);
    out.duration_ms = strtoul(doc["totlen"] | "0", nullptr, 10);
    out.polled_at_ms = millis();
    return Result::kOk;
  }

  // getMetaInfo's albumArtURI; a request of its own, so it's asked for after every poll.
  String artUrl() override {
    String reply;
    if (!request("getMetaInfo", reply)) return last_art_;
    JsonDocument doc;
    if (deserializeJson(doc, reply)) return last_art_;
    String url = doc["metaData"]["albumArtURI"] | "";
    if (!url.startsWith("http")) url = "";  // e.g. "un_known" for sources without art
    return last_art_ = url;
  }

  void send(Command command, const Poll&) override {
    static const char* const kCommands[] = {"setPlayerCmd:onepause", "setPlayerCmd:next", "setPlayerCmd:prev"};
    String reply;
    request(kCommands[static_cast<int>(command)], reply);
  }

  void disconnect() override {
    http_.end();
    tls_.stop();
  }

  uint32_t pollIntervalMs(Status) const override { return kPollMs; }

 private:
  void start() {
    started_ = true;
    host_ = settings::wiimHost();
    auto_host_ = host_.isEmpty();
    tls_.setInsecure();  // the WiiM only speaks HTTPS, with a self-signed certificate
    tls_.setHandshakeTimeout(kTimeoutMs / 1000);
    http_.setReuse(true);
    http_.setConnectTimeout(kTimeoutMs);
    http_.setTimeout(kTimeoutMs);
  }

  bool request(const char* command, String& reply) {
    String url = "https://" + host_ + "/httpapi.asp?command=";
    url += command;
    if (!http_.begin(tls_, url)) return false;
    int code = http_.GET();
    if (code != HTTP_CODE_OK) {
      Serial.printf("wiim: %s failed (%d %s)\n", command, code, http_.errorToString(code).c_str());
      http_.end();
      return false;
    }
    reply = http_.getString();
    http_.end();  // keeps the connection open for the next request (setReuse)
    return true;
  }

  Result failed() {
    if (++failures_ >= kFailuresBeforeRediscover && auto_host_) host_ = "";
    return Result::kFailed;
  }

  static Status parseStatus(const char* s) {
    // "load" is buffering the next track.
    if (!strcmp(s, "play") || !strcmp(s, "load")) return Status::kPlaying;
    if (!strcmp(s, "pause")) return Status::kPaused;
    return Status::kStopped;
  }

  // Picks the first WiiM on the network; false if there's none.
  bool findHost() {
    std::vector<wiim::Device> found = wiim::discover();
    if (found.empty()) {
      Serial.println("wiim: none found on the network");
      return false;
    }
    host_ = found[0].ip;
    failures_ = 0;
    Serial.printf("wiim: found %s at %s\n", found[0].name.c_str(), host_.c_str());
    disconnect();  // any open connection is to the old address
    return true;
  }

  bool started_ = false;
  String host_;  // from settings, or found on the network when that's empty
  bool auto_host_ = true;
  int failures_ = 0;
  String last_art_;
  WiFiClientSecure tls_;
  HTTPClient http_;
};

}  // namespace

Source* wiimSource() {
  static WiimSource source;
  return &source;
}

}  // namespace player

#endif  // FAKE_PLAYER
