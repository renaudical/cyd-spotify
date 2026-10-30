#pragma once

#include <lvgl.h>

// Semantic roles that screens style themselves with. Each character file (e.g. santa.h)
// defines its own named tokens and maps them onto these roles, so swapping characters
// never touches screen code.
struct Theme {
  const char* name;   // stable id, saved as the user's choice
  const char* label;  // shown in the theme picker

  lv_color_t background;
  lv_color_t text;
  lv_color_t accent;

  lv_color_t progress_track;
  lv_color_t progress_fill;

  lv_color_t button_border;
  lv_opa_t button_border_opa;
  lv_color_t button_pressed;
  lv_opa_t button_pressed_opa;
  lv_color_t icon;

  // Shown in the album art box when a source has no art or it fails to load.
  lv_color_t art_placeholder_bg;
  lv_color_t art_placeholder_fg;
  const lv_font_t* font_art_placeholder;  // must contain LV_SYMBOL_AUDIO

  const lv_font_t* font_title;
  const lv_font_t* font_meta;  // artist and album
  int32_t meta_letter_space;

  // Draws the character's clock screensaver onto an empty screen (art may bleed to the edges) and
  // returns the label the screensaver writes the time into.
  lv_obj_t* (*build_screensaver)(lv_obj_t* screen);
};

namespace theme {

// Loads the saved choice. Call before building any screen.
void begin();

// Every theme, in picker order (listed in themes.cpp).
size_t count();
const Theme& at(size_t index);

const Theme& active();
size_t activeIndex();
// Makes a theme active and saves the choice; screens built after this use it.
void select(size_t index);

}  // namespace theme
