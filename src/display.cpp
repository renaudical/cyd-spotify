#include "display.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace display {
namespace {

// LVGL's TFT_eSPI driver (lv_tft_espi.cpp) keeps its TFT_eSPI in a private struct whose only member is
// this pointer. Drawing through that same object matters: TFT_eSPI caches the address window, so a
// second instance would leave LVGL's cache stale.
struct TftEspiDriver {
  TFT_eSPI* tft;
};

TFT_eSPI* tft = nullptr;
SemaphoreHandle_t mutex = nullptr;

}  // namespace

void begin(lv_display_t* display) {
  tft = static_cast<TftEspiDriver*>(lv_display_get_driver_data(display))->tft;
  mutex = xSemaphoreCreateMutex();
}

void lock() { xSemaphoreTake(mutex, portMAX_DELAY); }
void unlock() { xSemaphoreGive(mutex); }

void startArea(int32_t x, int32_t y, int32_t w, int32_t h) {
  tft->startWrite();
  tft->setAddrWindow(x, y, w, h);
}

void push(const uint16_t* pixels, uint32_t count) {
  tft->pushColors(const_cast<uint16_t*>(pixels), count, true);  // the panel wants big-endian
}

void endArea() { tft->endWrite(); }

}  // namespace display
