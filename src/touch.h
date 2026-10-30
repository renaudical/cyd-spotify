#pragma once

#include <lvgl.h>

namespace touch {

struct State {
  bool pressed;
  int16_t raw_x;
  int16_t raw_y;
  int16_t raw_z;
};

// Starts the touch SPI bus and registers the LVGL pointer input device.
void begin();

// Last reading from the controller, before mapping to screen coordinates.
const State& state();

lv_indev_t* indev();

}  // namespace touch
