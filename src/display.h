#pragma once

#include <lvgl.h>
#include <stdint.h>

// Shared access to the panel. LVGL draws on the UI task while it holds the lock (main.cpp's loop); the
// player task takes it to draw the full-screen album art straight onto the panel, which is too big
// to go through LVGL's RAM.
namespace display {

// Call once, right after creating the LVGL display.
void begin(lv_display_t* display);

void lock();
void unlock();

// Direct drawing, bypassing LVGL; only while holding the lock. Coordinates are LVGL's (landscape,
// already rotated); pixels are LVGL's little-endian RGB565, pushed left to right, top to bottom.
void startArea(int32_t x, int32_t y, int32_t w, int32_t h);
void push(const uint16_t* pixels, uint32_t count);
void endArea();

}  // namespace display
