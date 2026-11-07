#include "album_art.h"

namespace album_art_theme {
namespace {

lv_obj_t* buildScreensaver(lv_obj_t* screen) {
  lv_obj_set_style_bg_color(screen, lv_color_hex(black), 0);
  lv_obj_t* time = lv_label_create(screen);
  lv_obj_center(time);
  lv_obj_set_style_text_font(time, font_clock, 0);
  lv_obj_set_style_text_color(time, lv_color_hex(white), 0);
  return time;
}

}  // namespace

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
    buildScreensaver,
    true,  // art_screensaver
};

}  // namespace album_art_theme
