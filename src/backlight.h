#pragma once

#include <stdint.h>

namespace backlight {

constexpr uint8_t kFull = 255;

// Takes over the backlight pin from TFT_eSPI with PWM; call after the display is created.
void begin();

// Animates to a brightness (0-255) through LVGL, so it must run on the LVGL thread.
void fadeTo(uint8_t level, uint32_t duration_ms);

}  // namespace backlight
