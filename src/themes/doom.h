#pragma once

#include "fonts/fonts.h"
#include "theme.h"

namespace doom {

// Character tokens, from the Figma "Player - Doom" frames. Screens should use the Theme roles, not
// these names directly.
constexpr uint32_t villain_red = 0xB13137;  // Figma background
constexpr uint32_t mask_black = 0x000000;   // the tag and clock
constexpr uint32_t snow_white = 0xFFFFFF;   // Figma "SnowWhite"
constexpr uint32_t gold = 0xE6B422;         // Figma "Gold"

constexpr const lv_font_t* font_title = &grift_semibold_14;  // Grift SemiBold 14
constexpr const lv_font_t* font_meta = &grift_medium_12;     // Grift Medium 12
constexpr int32_t meta_letter_space = 1;                     // Figma: 0.6px
constexpr const lv_font_t* font_clock = &grift_bold_24;      // Grift Bold 24

extern const Theme theme;

}  // namespace doom
