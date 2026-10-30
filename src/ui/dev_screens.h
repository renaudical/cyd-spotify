#pragma once

// Hardware test screens, only built with -DDEV_SCREENS.
// Each call builds a fresh screen, loads it, and frees the previous one.
// Call from outside LVGL event handlers (use lv_async_call when switching from a button).
namespace ui {

void showTouchTest();
void showCalibration();
void showSwatches();  // boot screen in DEV_SCREENS builds

}  // namespace ui
