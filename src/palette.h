#pragma once

#include <stdint.h>

// The handful of colors that stand for a piece of album art: the album art screensaver's dust, and
// later the LEDs.
namespace palette {

constexpr int kColors = 4;

struct Rgb {
  uint8_t r, g, b;
};

// Picks kColors colors from RGB565 pixels (little-endian, LVGL's layout), most common first. They're
// brightened enough to show up on black, since that's where they're used.
void fromArt(const uint16_t* pixels, int width, int height, Rgb out[kColors]);

}  // namespace palette
