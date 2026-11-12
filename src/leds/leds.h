#pragma once

#include "palette.h"

// The LED strand inside the Brickman figure, for the LED Lights theme: the album's colors drifting and
// breathing through the body while music plays, soft white twinkling otherwise. It runs on its own
// task, so the screen (and the player task drawing the album art) never makes it stutter.
namespace leds {

// Fades the LEDs in or out. Their pin is left alone until the first time they're switched on.
void setEnabled(bool enabled);

// Whether music is playing: album colors if so, white twinkling if not. The change crossfades.
void setPlaying(bool playing);

// The album's colors (from palette::fromArt()), or nullptr for a calm blue and purple when there's no
// art. Crossfades over about 2 s.
void setColors(const palette::Rgb* colors);

}  // namespace leds
