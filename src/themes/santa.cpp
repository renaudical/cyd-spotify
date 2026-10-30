#include "santa.h"

namespace santa {
namespace {

lv_obj_t* shape(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  return obj;
}

// Belt and buckle with the time inside, from the Figma "Clock - Holiday" frame.
lv_obj_t* buildScreensaver(lv_obj_t* screen) {
  lv_obj_set_style_bg_color(screen, lv_color_hex(coat_red), 0);

  lv_obj_t* belt = shape(screen, 0, 70, 320, 100);
  lv_obj_set_style_bg_color(belt, lv_color_hex(belt_black), 0);
  lv_obj_set_style_bg_opa(belt, LV_OPA_COVER, 0);

  lv_obj_t* buckle = shape(screen, 78, 30, 180, 180);
  lv_obj_set_style_border_color(buckle, lv_color_hex(buckle_gold), 0);
  lv_obj_set_style_border_width(buckle, 24, 0);
  lv_obj_set_style_border_opa(buckle, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(buckle, 8, 0);

  // Figma centers the text at x 172 in a 33px box at y 100. The clock font only has caps and digits,
  // so its line is 18px; y 107 keeps the glyphs where Figma draws them.
  lv_obj_t* time = lv_label_create(screen);
  lv_obj_set_pos(time, 24, 107);
  lv_obj_set_width(time, 296);
  lv_obj_set_style_text_align(time, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(time, font_clock, 0);
  lv_obj_set_style_text_color(time, lv_color_hex(buckle_gold), 0);
  return time;
}

}  // namespace

const Theme theme = {
    "santa",
    "Santa",
    lv_color_hex(coat_red),     // background
    lv_color_hex(fur_white),    // text
    lv_color_hex(buckle_gold),  // accent
    lv_color_hex(fur_white),    // progress_track
    lv_color_hex(buckle_gold),  // progress_fill
    lv_color_hex(fur_white),    // button_border
    LV_OPA_30,                  // button_border_opa (Figma: 0.5px at 50%, drawn as 1px)
    lv_color_hex(fur_white),    // button_pressed
    LV_OPA_20,                  // button_pressed_opa
    lv_color_hex(fur_white),    // icon
    lv_color_hex(belt_black),   // art_placeholder_bg
    lv_color_hex(buckle_gold),  // art_placeholder_fg
    &lv_font_montserrat_28,     // font_art_placeholder
    font_title,
    font_meta,
    meta_letter_space,
    buildScreensaver,
};

}  // namespace santa
