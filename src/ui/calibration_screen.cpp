#ifdef DEV_SCREENS
#include "dev_screens.h"

#include <lvgl.h>

#include "touch.h"

// Bezel-measurement grid. Deliberately unthemed: it needs fixed, high-contrast colors.
namespace ui {
namespace {

constexpr int32_t kGridStep = 5;
constexpr int32_t kLabelStep = 20;
// Labels sit this far in from each edge so they survive a typical bezel overlap.
constexpr int32_t kLabelInset = 14;

const lv_font_t* const kLabelFont = &lv_font_montserrat_10;

struct CalibrationUi {
  lv_obj_t* readout;
  lv_obj_t* cross_h;
  lv_obj_t* cross_v;
  lv_timer_t* timer;
};

// Lines every 5px: red, every 10px yellow, every 50px white; the last row/column is cyan.
lv_color_t lineColor(int32_t pos, int32_t size) {
  if (pos == size - 1) return lv_color_hex(0x00E5FF);
  if (pos % 50 == 0) return lv_color_hex(0xFFFFFF);
  if (pos % 10 == 0) return lv_color_hex(0xFFD400);
  return lv_color_hex(0xC03030);
}

void drawLine(lv_layer_t* layer, lv_draw_rect_dsc_t* dsc, lv_color_t color, int32_t x1, int32_t y1,
              int32_t x2, int32_t y2) {
  dsc->bg_color = color;
  lv_area_t area = {x1, y1, x2, y2};
  lv_draw_rect(layer, dsc, &area);
}

// Drawn in DRAW_MAIN_END so the grid sits over the background but under child objects.
void drawGrid(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  const int32_t w = lv_display_get_horizontal_resolution(nullptr);
  const int32_t h = lv_display_get_vertical_resolution(nullptr);

  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_opa = LV_OPA_COVER;
  dsc.radius = 0;

  for (int32_t x = 0; x < w; x += kGridStep) drawLine(layer, &dsc, lineColor(x, w), x, 0, x, h - 1);
  drawLine(layer, &dsc, lineColor(w - 1, w), w - 1, 0, w - 1, h - 1);
  for (int32_t y = 0; y < h; y += kGridStep) drawLine(layer, &dsc, lineColor(y, h), 0, y, w - 1, y);
  drawLine(layer, &dsc, lineColor(h - 1, h), 0, h - 1, w - 1, h - 1);
}

lv_obj_t* makeNumber(lv_obj_t* parent, int32_t value) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, kLabelFont, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_color(label, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(label, 1, 0);
  lv_label_set_text_fmt(label, "%d", (int)value);
  lv_obj_update_layout(label);
  return label;
}

// Numbers each labeled line by its pixel coordinate, along all four edges.
void addEdgeNumbers(lv_obj_t* scr, int32_t w, int32_t h) {
  // Labels within two steps of a corner are skipped so the edges don't collide.
  for (int32_t x = 2 * kLabelStep; x <= w - 2 * kLabelStep; x += kLabelStep) {
    lv_obj_t* top = makeNumber(scr, x);
    lv_obj_set_pos(top, x - lv_obj_get_width(top) / 2, kLabelInset);
    lv_obj_t* bottom = makeNumber(scr, x);
    lv_obj_set_pos(bottom, x - lv_obj_get_width(bottom) / 2, h - kLabelInset - lv_obj_get_height(bottom));
  }
  for (int32_t y = 2 * kLabelStep; y <= h - 2 * kLabelStep; y += kLabelStep) {
    lv_obj_t* left = makeNumber(scr, y);
    lv_obj_set_pos(left, kLabelInset, y - lv_obj_get_height(left) / 2);
    lv_obj_t* right = makeNumber(scr, y);
    lv_obj_set_pos(right, w - kLabelInset - lv_obj_get_width(right), y - lv_obj_get_height(right) / 2);
  }
}

lv_obj_t* makeCrossLine(lv_obj_t* parent, int32_t w, int32_t h) {
  lv_obj_t* line = lv_obj_create(parent);
  lv_obj_remove_style_all(line);
  lv_obj_set_size(line, w, h);
  lv_obj_set_style_bg_color(line, lv_color_hex(0xFF00FF), 0);
  lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
  lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
  return line;
}

void update(lv_timer_t* timer) {
  auto* ui = static_cast<CalibrationUi*>(lv_timer_get_user_data(timer));
  if (!touch::state().pressed) return;

  lv_point_t p;
  lv_indev_get_point(touch::indev(), &p);
  lv_label_set_text_fmt(ui->readout, "touch  x: %d  y: %d\nlong-press to go back", (int)p.x, (int)p.y);
  lv_obj_set_y(ui->cross_h, p.y);
  lv_obj_set_x(ui->cross_v, p.x);
  lv_obj_remove_flag(ui->cross_h, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(ui->cross_v, LV_OBJ_FLAG_HIDDEN);
}

void onDelete(lv_event_t* e) {
  auto* ui = static_cast<CalibrationUi*>(lv_event_get_user_data(e));
  lv_timer_delete(ui->timer);
  delete ui;
}

void onLongPress(lv_event_t*) {
  lv_async_call([](void*) { showTouchTest(); }, nullptr);
}

}  // namespace

void showCalibration() {
  const int32_t w = lv_display_get_horizontal_resolution(nullptr);
  const int32_t h = lv_display_get_vertical_resolution(nullptr);
  auto* ui = new CalibrationUi{};

  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(scr, drawGrid, LV_EVENT_DRAW_MAIN_END, nullptr);
  lv_obj_add_event_cb(scr, onLongPress, LV_EVENT_LONG_PRESSED, nullptr);

  addEdgeNumbers(scr, w, h);

  ui->readout = lv_label_create(scr);
  lv_obj_set_style_text_font(ui->readout, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ui->readout, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_align(ui->readout, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_color(ui->readout, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(ui->readout, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(ui->readout, 4, 0);
  lv_label_set_text(ui->readout, "tap to place crosshair\nlong-press to go back");
  lv_obj_center(ui->readout);

  ui->cross_h = makeCrossLine(scr, w, 1);
  ui->cross_v = makeCrossLine(scr, 1, h);

  ui->timer = lv_timer_create(update, 30, ui);
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, ui);

  lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

}  // namespace ui

#endif  // DEV_SCREENS
