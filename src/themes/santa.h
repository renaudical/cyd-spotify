#pragma once

#include "fonts/fonts.h"
#include "theme.h"

namespace santa {

// Character tokens, from the Figma file. Screens should use the Theme roles, not these names directly.
// Red and gold were picked on the device: Figma's #B34044 / #A38625 look pink and muddy on this panel.
constexpr uint32_t coat_red = 0xC8102E;     // Figma "SantaRed"
constexpr uint32_t fur_white = 0xFFFFFF;    // Figma "SnowWhite"
constexpr uint32_t buckle_gold = 0xE6B422;
constexpr uint32_t belt_black = 0x000000;

constexpr const lv_font_t* font_title = &grift_semibold_14;  // Grift SemiBold 14
constexpr const lv_font_t* font_meta = &grift_medium_12;     // Grift Medium 12
constexpr int32_t meta_letter_space = 1;                     // Figma: 0.6px
constexpr const lv_font_t* font_clock = &grift_semibold_26;  // Grift SemiBold 26

extern const Theme theme;

}  // namespace santa
