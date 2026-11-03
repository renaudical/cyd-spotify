#include "doom.h"

#include "assets/assets.h"

namespace doom {
namespace {

// LVGL 9.2 copies an A8 image into its 32 KB memory pool every time it draws it, and the 40 KB tag
// doesn't fit, so it's drawn as strips of rows that point into the image in flash.
constexpr int32_t kStripRows = 16;
constexpr int32_t kStrips = (185 + kStripRows - 1) / kStripRows;  // doom_tag is 215x185
lv_image_dsc_t strips[kStrips];

// The MF DOOM tag (a one-color image, tinted black) with the time in the top-right corner, from the
// second Figma "Player - Doom" frame.
lv_obj_t* buildScreensaver(lv_obj_t* screen) {
  lv_obj_set_style_bg_color(screen, lv_color_hex(villain_red), 0);
  const lv_image_header_t& header = doom_tag.header;
  for (int32_t i = 0; i < kStrips; i++) {
    int32_t y = i * kStripRows;
    lv_image_dsc_t& strip = strips[i];
    strip.header = header;
    strip.header.h = LV_MIN(kStripRows, header.h - y);
    strip.data = doom_tag.data + y * header.stride;
    strip.data_size = strip.header.h * header.stride;

    lv_obj_t* part = lv_image_create(screen);
    lv_image_set_src(part, &strip);
    lv_obj_set_pos(part, 53, 27 + y);  // cropped to the tag; Figma's image frame is at (12, 24)
    lv_obj_set_style_image_recolor(part, lv_color_hex(mask_black), 0);
    lv_obj_set_style_image_recolor_opa(part, LV_OPA_COVER, 0);
  }

  lv_obj_t* time = lv_label_create(screen);
  // The clock font's line is cap height, so this is where Figma's caps start.
  lv_obj_align(time, LV_ALIGN_TOP_RIGHT, -12, 30);
  lv_obj_set_style_text_font(time, font_clock, 0);
  lv_obj_set_style_text_color(time, lv_color_hex(mask_black), 0);
  return time;
}

}  // namespace

const Theme theme = {
    "doom",
    "MF DOOM",
    lv_color_hex(villain_red),  // background
    lv_color_hex(snow_white),   // text
    lv_color_hex(gold),         // accent
    lv_color_hex(snow_white),   // progress_track
    lv_color_hex(gold),         // progress_fill
    lv_color_hex(snow_white),   // button_border
    LV_OPA_30,                  // button_border_opa (Figma: 0.5px at 50%, drawn as 1px)
    lv_color_hex(snow_white),   // button_pressed
    LV_OPA_20,                  // button_pressed_opa
    lv_color_hex(snow_white),   // icon
    lv_color_hex(mask_black),   // art_placeholder_bg
    lv_color_hex(snow_white),   // art_placeholder_fg
    &symbol_audio_28,           // font_art_placeholder
    font_title,
    font_meta,
    meta_letter_space,
    buildScreensaver,
};

}  // namespace doom
