#include "spider.h"

#include "assets/assets.h"

namespace spider {
namespace {

// The web is a full-screen image from the Figma "Clock - Spider" frame (too detailed for shapes);
// the time sits on a red pill that hides the strands behind it.
lv_obj_t* buildScreensaver(lv_obj_t* screen) {
  lv_obj_set_style_bg_color(screen, lv_color_hex(suit_blue), 0);
  lv_obj_t* web = lv_image_create(screen);
  lv_image_set_src(web, &spider_screensaver);
  lv_obj_set_pos(web, 0, 0);

  lv_obj_t* time = lv_label_create(screen);
  lv_obj_align(time, LV_ALIGN_CENTER, 0, 25);  // Figma: centered below 50px of top padding
  lv_obj_set_style_text_font(time, font_clock, 0);
  lv_obj_set_style_text_color(time, lv_color_hex(web_white), 0);
  lv_obj_set_style_bg_color(time, lv_color_hex(suit_red), 0);
  lv_obj_set_style_bg_opa(time, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(time, LV_RADIUS_CIRCLE, 0);  // Figma: 18px on a ~25px pill
  // The clock font's line is 14px; Figma's pill is the 21px font's ~25px line.
  lv_obj_set_style_pad_ver(time, 5, 0);
  return time;
}

}  // namespace

const Theme theme = {
    "spider",
    "Spider",
    lv_color_hex(suit_blue),  // background
    lv_color_hex(web_white),  // text
    lv_color_hex(gold),       // accent
    lv_color_hex(web_white),  // progress_track
    lv_color_hex(gold),       // progress_fill
    lv_color_hex(web_white),  // button_border
    LV_OPA_30,                // button_border_opa (Figma: 0.5px at 50%, drawn as 1px)
    lv_color_hex(web_white),  // button_pressed
    LV_OPA_20,                // button_pressed_opa
    lv_color_hex(web_white),  // icon
    lv_color_hex(suit_red),   // art_placeholder_bg
    lv_color_hex(web_white),  // art_placeholder_fg
    &symbol_audio_28,          // font_art_placeholder
    font_title,
    font_meta,
    meta_letter_space,
    buildScreensaver,
};

}  // namespace spider
