#pragma once

#include <lvgl.h>
#include <stdint.h>

// What's playing, and the transport controls. The UI only talks to this interface.
// wiim_player.cpp implements it against the real WiiM; fake_player.cpp (build with -DFAKE_PLAYER)
// is a fixed playlist of UI edge cases.
namespace player {

enum class Status { kStopped, kPaused, kPlaying };

// Whether the track fields can be trusted; anything but kOk means the UI shows a problem message instead.
enum class Link { kOk, kWifiConnecting, kWifiLost, kWiimConnecting, kWiimUnreachable };

struct State {
  // Bumped whenever anything other than the position changes, so the UI can skip redraws.
  uint32_t version;
  Link link;
  Status status;
  char title[128];
  char artist[96];
  char album[128];
  uint32_t position_ms;
  uint32_t duration_ms;  // 0 when unknown (e.g. radio streams)
  const lv_image_dsc_t* art;  // nullptr: show the placeholder
};

void begin();
// Call regularly from the main loop.
void update();
const State& state();

void togglePlayPause();
void next();
void previous();

}  // namespace player
