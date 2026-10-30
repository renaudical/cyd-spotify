#include "ui.h"

#include "backlight.h"
#include "themes/theme.h"

namespace ui {
namespace {

constexpr uint32_t kIdleTimeoutMs = 10000;
constexpr uint8_t kScreensaverBrightness = 40;
constexpr uint32_t kDimFadeMs = 800;
constexpr uint32_t kWakeFadeMs = 150;

lv_obj_t* now_playing = nullptr;
lv_obj_t* screensaver = nullptr;
lv_obj_t* picker = nullptr;  // only exists while open

void openPicker();

void wake() {
  lv_screen_load(now_playing);
  backlight::fadeTo(backlight::kFull, kWakeFadeMs);
}

void buildScreens() {
  now_playing = createNowPlaying(openPicker);
  screensaver = createScreensaver(wake);
}

void closePicker() {
  lv_screen_load(now_playing);
  lv_obj_delete(picker);
  picker = nullptr;
}

// Screens take their colors when they're built, so a new theme means building them again.
void pickTheme(size_t index) {
  lv_obj_t* old_screens[] = {now_playing, screensaver, picker};
  theme::select(index);
  buildScreens();
  lv_screen_load(now_playing);
  for (lv_obj_t* old : old_screens) lv_obj_delete(old);
  picker = nullptr;
}

void openPicker() {
  picker = createThemePicker(pickTheme, closePicker);
  lv_screen_load(picker);
}

void checkIdle(lv_timer_t*) {
  if (lv_display_get_inactive_time(nullptr) < kIdleTimeoutMs) return;
  if (picker) closePicker();  // the screensaver follows on the next check
  else if (lv_screen_active() == now_playing) {
    lv_screen_load(screensaver);
    backlight::fadeTo(kScreensaverBrightness, kDimFadeMs);
  }
}

}  // namespace

void begin() {
  buildScreens();
  lv_screen_load(now_playing);
  lv_timer_create(checkIdle, 250, nullptr);
}

}  // namespace ui
