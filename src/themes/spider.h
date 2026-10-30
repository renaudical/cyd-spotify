#pragma once

#include "fonts/fonts.h"
#include "theme.h"

namespace spider {

// Character tokens, from the Figma "Player - Spider" and "Clock - Spider" frames. Screens should use
// the Theme roles, not these names directly.
constexpr uint32_t suit_blue = 0x1044C8;  // Figma "Spider"
constexpr uint32_t suit_red = 0xC8102E;   // the web in the screensaver art
constexpr uint32_t web_white = 0xFFFFFF;  // Figma "SnowWhite"
constexpr uint32_t gold = 0xE6B422;       // Figma "Gold"

constexpr const lv_font_t* font_title = &grift_semibold_14;  // Grift SemiBold 14
constexpr const lv_font_t* font_meta = &grift_medium_12;     // Grift Medium 12
constexpr int32_t meta_letter_space = 1;                     // Figma: 0.6px
constexpr const lv_font_t* font_clock = &grift_semibold_21;  // Grift SemiBold 21

extern const Theme theme;

}  // namespace spider
