#pragma once

#include "fonts/fonts.h"
#include "theme.h"

namespace album_art_theme {

// No character: black, so the album art is the color. Its screensaver is the album art itself (see
// Theme::art_screensaver); when nothing plays, white dust drifts across a black screen instead.
constexpr uint32_t black = 0x000000;
constexpr uint32_t white = 0xFFFFFF;
constexpr uint32_t gray = 0x8A8A8A;
constexpr uint32_t charcoal = 0x262626;

constexpr const lv_font_t* font_title = &grift_semibold_14;  // Grift SemiBold 14
constexpr const lv_font_t* font_meta = &grift_medium_12;     // Grift Medium 12
constexpr int32_t meta_letter_space = 1;

// The screensaver for when nothing plays, shared with the LED Lights theme. No clock.
lv_obj_t* buildQuietScreensaver(lv_obj_t* screen);

extern const Theme theme;

}  // namespace album_art_theme
