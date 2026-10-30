#ifdef DEV_SCREENS
#include "dev_screens.h"

#include <lvgl.h>

#include "fonts/fonts.h"

// Candidate colors shown side by side on the real panel, which shifts colors a lot.
// Deliberately unthemed: it compares raw values.
namespace ui {
namespace {

constexpr uint32_t kReds[] = {0xB34044, 0xC8102E, 0xB80F0A, 0xD00000, 0xA50000};
constexpr uint32_t kGolds[] = {0xA38625, 0xC9A227, 0xD4AF37, 0xE6B422};
constexpr uint32_t kCurrentGold = 0xA38625;
constexpr int32_t kRedHeight = 150;

lv_obj_t* box(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  return obj;
}

lv_obj_t* text(lv_obj_t* parent, const lv_font_t* font, uint32_t color, const char* str, int32_t y) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_label_set_text(label, str);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
  return label;
}

void onTouchTestClicked(lv_event_t*) {
  lv_async_call([](void*) { showTouchTest(); }, nullptr);
}

}  // namespace

void showSwatches() {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Reds, each with white text and the progress bar as they appear on Now Playing.
  const int32_t red_w = 320 / 5;
  for (size_t i = 0; i < 5; i++) {
    lv_obj_t* col = box(scr, i * red_w, 0, red_w, kRedHeight, kReds[i]);
    char hex[8];
    lv_snprintf(hex, sizeof(hex), "%06X", (unsigned)kReds[i]);
    text(col, &lv_font_montserrat_14, 0xFFFFFF, i == 0 ? "now" : "", 30);
    text(col, &grift_semibold_14, 0xFFFFFF, "Santa", 52);
    text(col, &grift_medium_12, 0xFFFFFF, hex, 74);
    lv_obj_t* track = box(col, 8, 104, red_w - 16, 4, 0xFFFFFF);
    lv_obj_t* fill = box(track, 0, 0, (red_w - 16) / 2, 4, kCurrentGold);
    lv_obj_set_style_radius(fill, 4, 0);
  }

  // Golds on the black belt, as on the screensaver.
  const int32_t gold_w = 320 / 4;
  for (size_t i = 0; i < 4; i++) {
    lv_obj_t* swatch = box(scr, i * gold_w + 8, kRedHeight + 14, gold_w - 16, 44, kGolds[i]);
    lv_obj_set_style_radius(swatch, 6, 0);
    char hex[8];
    lv_snprintf(hex, sizeof(hex), "%06X", (unsigned)kGolds[i]);
    lv_obj_t* label = text(swatch, &grift_medium_12, 0x000000, hex, 0);
    lv_obj_center(label);
  }
  lv_obj_t* hint = text(scr, &grift_medium_12, 0x808080, "first gold = now", 0);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 16, -8);

  lv_obj_t* button = lv_button_create(scr);
  lv_obj_set_size(button, 96, 22);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x404040), 0);
  lv_obj_align(button, LV_ALIGN_BOTTOM_RIGHT, -16, -4);
  lv_obj_add_event_cb(button, onTouchTestClicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* button_label = text(button, &grift_medium_12, 0xFFFFFF, "Touch test", 0);
  lv_obj_center(button_label);

  lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

}  // namespace ui

#endif  // DEV_SCREENS
