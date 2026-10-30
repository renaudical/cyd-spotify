#include "themes/theme.h"
#include "ui.h"
#include "net/net.h"
#include "updater/updater.h"

namespace ui {
namespace {

// Inside the 12px side / 24px top and bottom safe zone, like the Figma "Player" frame.
constexpr int32_t kMarginX = 16, kTop = 24, kWidth = 320 - 2 * kMarginX;
constexpr int32_t kHeaderH = 32, kRowH = 44, kRowGap = 8, kSwatch = 28;

void (*pick_cb)(size_t) = nullptr;
void (*close_cb)() = nullptr;

// Both callbacks delete this screen, so they run after the event handler has returned.
void onRow(lv_event_t* e) {
  lv_async_call([](void* index) { pick_cb(reinterpret_cast<size_t>(index)); }, lv_event_get_user_data(e));
}

void onBack(lv_event_t*) {
  lv_async_call([](void*) { close_cb(); }, nullptr);
}

lv_obj_t* makeButton(lv_obj_t* parent) {
  const Theme& th = theme::active();
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_remove_style_all(button);
  lv_obj_set_style_radius(button, 4, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, th.button_border, 0);
  lv_obj_set_style_border_opa(button, th.button_border_opa, 0);
  lv_obj_set_style_bg_color(button, th.button_pressed, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, th.button_pressed_opa, LV_STATE_PRESSED);
  return button;
}

lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, const char* text) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, theme::active().text, 0);
  lv_label_set_text(label, text);
  return label;
}

// A small preview of a theme: its background with its progress color along the bottom.
void makeSwatch(lv_obj_t* parent, const Theme& t) {
  lv_obj_t* swatch = lv_obj_create(parent);
  lv_obj_remove_style_all(swatch);
  lv_obj_set_size(swatch, kSwatch, kSwatch);
  lv_obj_align(swatch, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_set_style_radius(swatch, 4, 0);
  lv_obj_set_style_bg_color(swatch, t.background, 0);
  lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(swatch, 1, 0);
  lv_obj_set_style_border_color(swatch, t.text, 0);
  lv_obj_set_style_clip_corner(swatch, true, 0);
  lv_obj_remove_flag(swatch, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* bar = lv_obj_create(swatch);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, kSwatch, 6);
  lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(bar, t.progress_fill, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
}

}  // namespace

lv_obj_t* createThemePicker(void (*on_pick)(size_t), void (*on_close)()) {
  pick_cb = on_pick;
  close_cb = on_close;
  const Theme& th = theme::active();

  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, th.background, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = makeLabel(scr, th.font_title, "Theme");
  lv_obj_set_pos(title, kMarginX, kTop + (kHeaderH - lv_font_get_line_height(th.font_title)) / 2);

  lv_obj_t* back = makeButton(scr);
  lv_obj_set_size(back, 64, kHeaderH);
  lv_obj_set_pos(back, 320 - kMarginX - 64, kTop);
  lv_obj_center(makeLabel(back, th.font_meta, "Back"));

  // Version and the settings page's address, in the bottom margin.
  String address = net::address();
  String footer = String("Version ") + updater::currentVersion();
  if (address.length()) footer += "  \xc2\xb7  Settings: " + address;
  lv_obj_t* version = makeLabel(scr, th.font_meta, footer.c_str());
  lv_obj_set_style_text_opa(version, LV_OPA_60, 0);
  lv_obj_align(version, LV_ALIGN_BOTTOM_RIGHT, -kMarginX, -6);
  lv_obj_add_event_cb(back, onBack, LV_EVENT_CLICKED, nullptr);

  // Rows scroll if there are more themes than fit.
  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_remove_style_all(list);
  lv_obj_set_pos(list, kMarginX, kTop + kHeaderH + kRowGap);
  lv_obj_set_size(list, kWidth, 240 - kTop - (kTop + kHeaderH + kRowGap));
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, kRowGap, 0);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

  for (size_t i = 0; i < theme::count(); i++) {
    const Theme& t = theme::at(i);
    lv_obj_t* row = makeButton(list);
    lv_obj_set_size(row, kWidth, kRowH);
    lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(row, onRow, LV_EVENT_CLICKED, reinterpret_cast<void*>(i));
    makeSwatch(row, t);
    lv_obj_align(makeLabel(row, th.font_title, t.label), LV_ALIGN_LEFT_MID, 8 + kSwatch + 12, 0);
    if (i == theme::activeIndex()) {
      lv_obj_align(makeLabel(row, &lv_font_montserrat_14, LV_SYMBOL_OK), LV_ALIGN_RIGHT_MID, -12, 0);
    }
  }
  return scr;
}

}  // namespace ui
