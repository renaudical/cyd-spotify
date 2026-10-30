// Stand-in for the WiiM (build with -DFAKE_PLAYER): a fixed playlist that exercises the UI's edge cases.
// Next/previous cycle through it; play/pause stops the clock.
#ifdef FAKE_PLAYER
#include <Arduino.h>

#include "assets/assets.h"
#include "net/net.h"
#include "player.h"

namespace player {
namespace {

struct FakeTrack {
  Status status;
  const char* title;
  const char* artist;
  const char* album;
  uint32_t duration_ms;
  uint32_t start_ms;
  const lv_image_dsc_t* art;
};

const FakeTrack kTracks[] = {
    // Matches the Figma frame.
    {Status::kPlaying, "Christmas Time is Here - Instrumental", "Vince Guaraldi Trio",
     "A Charlie Brown Christmas [2012 Remastered & expanded Edition]", 365000, 54000, &demo_art_guaraldi},
    // Long text everywhere, no art (like a Bluetooth source).
    {Status::kPlaying,
     "Rockin' Around the Christmas Tree (Extended Holiday Version With a Title That Just Keeps Going)",
     "Brenda Lee, The Anita Kerr Singers & The Nashville Studio Orchestra",
     "The Absolutely Complete Christmas Collection: Every Song Ever Recorded, Volume Seventeen", 190000, 170000,
     nullptr},
    // Accents and typographic punctuation.
    {Status::kPlaying, "Petit Papa Noël \xe2\x80\x93 L\xe2\x80\x99hiver", "Céline Dion",
     "Chants et contes de Noël\xe2\x80\xa6", 214000, 0, nullptr},
    // Nothing playing.
    {Status::kStopped, "", "", "", 0, 0, nullptr},
};
constexpr size_t kTrackCount = sizeof(kTracks) / sizeof(kTracks[0]);

State current = {};
size_t index = 0;
uint32_t last_tick_ms = 0;

void load(size_t i) {
  const FakeTrack& t = kTracks[i];
  index = i;
  current.status = t.status;
  strlcpy(current.title, t.title, sizeof(current.title));
  strlcpy(current.artist, t.artist, sizeof(current.artist));
  strlcpy(current.album, t.album, sizeof(current.album));
  current.duration_ms = t.duration_ms;
  current.position_ms = t.start_ms;
  current.art = t.art;
  current.version++;
}

}  // namespace

void begin() {
  load(0);
  last_tick_ms = millis();
}

void update() {
  // Wi-Fi is real even though the tracks are fake, so the problem messages can be tested.
  Link link = Link::kOk;
  if (net::wifi() == net::Wifi::kConnecting) link = Link::kWifiConnecting;
  if (net::wifi() == net::Wifi::kLost) link = Link::kWifiLost;
  if (link != current.link) {
    current.link = link;
    current.version++;
  }

  uint32_t now = millis();
  if (current.status == Status::kPlaying) {
    current.position_ms += now - last_tick_ms;
    if (current.duration_ms && current.position_ms >= current.duration_ms) next();
  }
  last_tick_ms = now;
}

const State& state() { return current; }

void togglePlayPause() {
  if (current.status == Status::kStopped) return;
  current.status = current.status == Status::kPlaying ? Status::kPaused : Status::kPlaying;
  current.version++;
}

void next() { load((index + 1) % kTrackCount); }

void previous() { load((index + kTrackCount - 1) % kTrackCount); }

bool suspend() { return true; }

void resume() {}

}  // namespace player

#endif  // FAKE_PLAYER
