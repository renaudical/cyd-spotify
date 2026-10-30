#pragma once

#include <lvgl.h>
#include <stdint.h>

// What's playing, and the transport controls. The UI only talks to this interface.
// player_core.cpp implements it on top of a music source (wiim_source.cpp or spotify_source.cpp, picked
// in settings); fake_player.cpp (build with -DFAKE_PLAYER) is a fixed playlist of UI edge cases.
namespace player {

enum class Status { kStopped, kPaused, kPlaying };

// Whether the track fields can be trusted; anything but kOk means the UI shows a problem message instead.
enum class Link {
  kOk,
  kWifiConnecting,
  kWifiLost,
  kConnecting,   // to the source
  kUnreachable,  // the source stopped answering
  kNotFound,     // no WiiM on the network
  kNeedsSetup,   // Spotify isn't signed in yet
};

struct State {
  // Bumped whenever anything other than the position changes, so the UI can skip redraws.
  uint32_t version;
  Link link;
  const char* source;  // "WiiM" or "Spotify", for the messages
  Status status;
  char title[128];
  char artist[96];
  char album[128];
  uint32_t position_ms;
  uint32_t duration_ms;  // 0 when unknown (e.g. radio streams)
  const lv_image_dsc_t* art;  // nullptr: show the placeholder
};

void begin();
// Takes the latest poll. Call on the UI task right before reading state() and applying it (Now
// Playing's refresh does), since it also tells the player when the UI has let go of the art buffer.
void update();
const State& state();

void togglePlayPause();
void next();
void previous();

}  // namespace player
