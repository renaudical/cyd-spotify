#include "touch.h"

#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "board.h"

namespace touch {
namespace {

SPIClass touch_spi(VSPI);
XPT2046_Touchscreen ts(board::kTouchCs, board::kTouchIrq);
State current = {};
lv_indev_t* pointer = nullptr;

int32_t mapAxis(int32_t raw, int32_t raw_min, int32_t raw_max, int32_t size) {
  return constrain(map(raw, raw_min, raw_max, 0, size - 1), 0, size - 1);
}

bool inDeadZone(int32_t x, int32_t y) {
  return x < board::kTouchDeadZone || x >= board::kNativeWidth - board::kTouchDeadZone ||
         y < board::kTouchDeadZone || y >= board::kNativeHeight - board::kTouchDeadZone;
}

// Reports points in the panel's native portrait orientation; LVGL applies the display rotation.
void read(lv_indev_t*, lv_indev_data_t* data) {
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    int32_t x = mapAxis(p.x, board::kTouchRawXMin, board::kTouchRawXMax, board::kNativeWidth);
    int32_t y = mapAxis(p.y, board::kTouchRawYMin, board::kTouchRawYMax, board::kNativeHeight);
    if (!inDeadZone(x, y)) {
      current = {true, p.x, p.y, p.z};
      data->point.x = x;
      data->point.y = y;
      data->state = LV_INDEV_STATE_PRESSED;
      return;
    }
  }
  current.pressed = false;
  data->state = LV_INDEV_STATE_RELEASED;
}

}  // namespace

void begin() {
  touch_spi.begin(board::kTouchSclk, board::kTouchMiso, board::kTouchMosi, board::kTouchCs);
  ts.begin(touch_spi);
  ts.setRotation(2);

  pointer = lv_indev_create();
  lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(pointer, read);
}

const State& state() { return current; }

lv_indev_t* indev() { return pointer; }

}  // namespace touch
