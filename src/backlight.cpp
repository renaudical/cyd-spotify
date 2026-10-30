#include "backlight.h"

#include <Arduino.h>
#include <lvgl.h>

#include "board.h"

namespace backlight {
namespace {

constexpr uint8_t kChannel = 0;
uint8_t current = kFull;

void set(void*, int32_t level) {
  current = level;
  ledcWrite(kChannel, level);
}

}  // namespace

void begin() {
  ledcSetup(kChannel, 5000, 8);
  ledcAttachPin(board::kBacklightPin, kChannel);
  ledcWrite(kChannel, current);
}

void fadeTo(uint8_t level, uint32_t duration_ms) {
  lv_anim_delete(&current, nullptr);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, &current);
  lv_anim_set_exec_cb(&a, set);
  lv_anim_set_values(&a, current, level);
  lv_anim_set_duration(&a, duration_ms);
  lv_anim_start(&a);
}

}  // namespace backlight
