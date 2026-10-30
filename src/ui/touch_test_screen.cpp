#ifdef DEV_SCREENS
#include "dev_screens.h"

#include <lvgl.h>

#include "themes/theme.h"
#include "touch.h"

namespace ui {
namespace {

constexpr int32_t kDotSize = 14;

struct TouchTestUi {
  lv_obj_t* coords;
  lv_obj_t* raw;
  lv_obj_t* dot;
  lv_timer_t* timer;
};

void update(lv_timer_t* timer) {
  auto* ui = static_cast<TouchTestUi*>(lv_timer_get_user_data(timer));
  const touch::State& s = touch::state();
  if (!s.pressed) {
    lv_obj_add_flag(ui->dot, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_point_t p;
  lv_indev_get_point(touch::indev(), &p);
  lv_label_set_text_fmt(ui->coords, "x: %d   y: %d", (int)p.x, (int)p.y);
  lv_label_set_text_fmt(ui->raw, "raw  x: %d   y: %d   z: %d", s.raw_x, s.raw_y, s.raw_z);
  lv_obj_set_pos(ui->dot, p.x - kDotSize / 2, p.y - kDotSize / 2);
  lv_obj_remove_flag(ui->dot, LV_OBJ_FLAG_HIDDEN);
}

void onDelete(lv_event_t* e) {
  auto* ui = static_cast<TouchTestUi*>(lv_event_get_user_data(e));
  lv_timer_delete(ui->timer);
  delete ui;
}

void onCalibrationClicked(lv_event_t*) {
  lv_async_call([](void*) { showCalibration(); }, nullptr);
}

void onColorsClicked(lv_event_t*) {
  lv_async_call([](void*) { showSwatches(); }, nullptr);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, const char* text) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_label_set_text(label, text);
  return label;
}

}  // namespace

void showTouchTest() {
  const Theme& th = theme::active();
  auto* ui = new TouchTestUi{};

  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, th.background, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = makeLabel(scr, &lv_font_montserrat_14, th.accent, "TOUCH TEST");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  ui->coords = makeLabel(scr, &lv_font_montserrat_28, th.text, "touch the screen");
  lv_obj_align(ui->coords, LV_ALIGN_CENTER, 0, -24);

  ui->raw = makeLabel(scr, &lv_font_montserrat_10, th.text, "raw  -");
  lv_obj_align(ui->raw, LV_ALIGN_CENTER, 0, 12);

  lv_obj_t* button = lv_button_create(scr);
  lv_obj_set_style_bg_color(button, th.accent, 0);
  lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 50, -16);
  lv_obj_add_event_cb(button, onCalibrationClicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* button_label = makeLabel(button, &lv_font_montserrat_14, th.text, "Calibration grid");
  lv_obj_center(button_label);

  lv_obj_t* colors = lv_button_create(scr);
  lv_obj_set_style_bg_color(colors, th.accent, 0);
  lv_obj_align(colors, LV_ALIGN_BOTTOM_MID, -80, -16);
  lv_obj_add_event_cb(colors, onColorsClicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* colors_label = makeLabel(colors, &lv_font_montserrat_14, th.text, "Colors");
  lv_obj_center(colors_label);

  // Created last so it draws above everything; not clickable so it never blocks the button.
  ui->dot = lv_obj_create(scr);
  lv_obj_remove_style_all(ui->dot);
  lv_obj_set_size(ui->dot, kDotSize, kDotSize);
  lv_obj_set_style_radius(ui->dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ui->dot, th.accent, 0);
  lv_obj_set_style_bg_opa(ui->dot, LV_OPA_COVER, 0);
  lv_obj_remove_flag(ui->dot, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui->dot, LV_OBJ_FLAG_HIDDEN);

  ui->timer = lv_timer_create(update, 30, ui);
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, ui);

  lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

}  // namespace ui

#endif  // DEV_SCREENS
