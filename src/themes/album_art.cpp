#include "album_art.h"

#include "ui/ui.h"

namespace album_art_theme {

lv_obj_t* buildQuietScreensaver(lv_obj_t* screen) {
  lv_obj_set_style_bg_color(screen, lv_color_hex(black), 0);
  ui::addDust(screen);
  return nullptr;
}

const Theme theme = {
    "album_art",
    "Album Art",
    lv_color_hex(black),     // background
    lv_color_hex(white),     // text
    lv_color_hex(white),     // accent
    lv_color_hex(charcoal),  // progress_track
    lv_color_hex(white),     // progress_fill
    lv_color_hex(white),     // button_border
    LV_OPA_30,               // button_border_opa
    lv_color_hex(white),     // button_pressed
    LV_OPA_20,               // button_pressed_opa
    lv_color_hex(white),     // icon
    lv_color_hex(charcoal),  // art_placeholder_bg
    lv_color_hex(gray),      // art_placeholder_fg
    &symbol_audio_28,         // font_art_placeholder
    font_title,
    font_meta,
    meta_letter_space,
    buildQuietScreensaver,
    true,   // art_screensaver
    true,   // art_backdrop
    false,  // leds
};

}  // namespace album_art_theme
