#include <Arduino.h>
#include <lvgl.h>

#include "backlight.h"
#include "board.h"
#include "net/net.h"
#include "player/player.h"
#include "themes/theme.h"
#include "touch.h"
#include "ui/dev_screens.h"
#include "ui/ui.h"

namespace {

// Partial render buffer: 1/10 of the screen at 16-bit color.
constexpr size_t kDrawBufBytes = board::kNativeWidth * board::kNativeHeight / 10 * (LV_COLOR_DEPTH / 8);
uint32_t draw_buf[kDrawBufBytes / 4];

uint32_t tick() { return millis(); }

}  // namespace

void setup() {
  Serial.begin(115200);

  lv_init();
  lv_tick_set_cb(tick);

  lv_display_t* display = lv_tft_espi_create(board::kNativeWidth, board::kNativeHeight, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);

  backlight::begin();
  touch::begin();
  theme::begin();

  Serial.printf("Now Playing Brickman: theme '%s', display %dx%d\n", theme::active().name,
                (int)lv_display_get_horizontal_resolution(display), (int)lv_display_get_vertical_resolution(display));

#ifdef DEV_SCREENS
  ui::showSwatches();
#else
  net::begin();
  player::begin();
  ui::begin();
#endif
}

void loop() {
#ifndef DEV_SCREENS
  player::update();
#endif
  lv_timer_handler();
  delay(5);
}
