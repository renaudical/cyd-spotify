#pragma once

#include <stdint.h>

// ESP32-2432S028R hardware constants. Display pins live in platformio.ini (TFT_eSPI build flags).
namespace board {

// The ILI9341 panel is natively portrait; main.cpp rotates LVGL to 320x240 landscape.
constexpr int32_t kNativeWidth = 240;
constexpr int32_t kNativeHeight = 320;

// Also TFT_BL in platformio.ini; backlight.cpp takes it over for PWM dimming.
constexpr uint8_t kBacklightPin = 21;

// XPT2046 touch controller, on its own SPI bus (VSPI), not shared with the display.
constexpr int8_t kTouchSclk = 25;
constexpr int8_t kTouchMosi = 32;
constexpr int8_t kTouchMiso = 39;
constexpr int8_t kTouchCs = 33;
constexpr int8_t kTouchIrq = 36;

// Touches this close to any edge are ignored: the bezel covers that strip and, pressing on the
// resistive panel, causes constant phantom touches there. No control is within 24px of an edge.
constexpr int32_t kTouchDeadZone = 12;

// Raw XPT2046 readings at the panel edges. Tune these using the touch test screen.
constexpr int16_t kTouchRawXMin = 200;
constexpr int16_t kTouchRawXMax = 3700;
constexpr int16_t kTouchRawYMin = 240;
constexpr int16_t kTouchRawYMax = 3800;

}  // namespace board
