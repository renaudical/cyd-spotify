#include "ui.h"

#include <stdio.h>

#include "backlight.h"
#include "net/net.h"
#include "themes/theme.h"
#include "updater/updater.h"

namespace ui {
namespace {

constexpr uint32_t kIdleTimeoutMs = 10000;
constexpr uint8_t kScreensaverBrightness = 40;
constexpr uint32_t kDimFadeMs = 800;
constexpr uint32_t kWakeFadeMs = 150;

lv_obj_t* now_playing = nullptr;
lv_obj_t* screensaver = nullptr;
lv_obj_t* picker = nullptr;  // only exists while open
lv_obj_t* setup = nullptr;  // only exists while the setup network is up
lv_obj_t* updating = nullptr;  // only exists while an update installs
lv_obj_t* updating_label = nullptr;

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

// Takes over the display while an update downloads; the device restarts when it's done.
void showUpdating() {
  if (!updating) {
    const Theme& th = theme::active();
    updating = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(updating, th.background, 0);
    lv_obj_remove_flag(updating, LV_OBJ_FLAG_SCROLLABLE);
    updating_label = lv_label_create(updating);
    lv_obj_set_style_text_font(updating_label, th.font_title, 0);
    lv_obj_set_style_text_color(updating_label, th.text, 0);
    lv_obj_center(updating_label);
    lv_screen_load(updating);
    backlight::fadeTo(backlight::kFull, kWakeFadeMs);
  }
  char text[48];
  snprintf(text, sizeof(text), "Updating to %s\u2026 %u%%", updater::targetVersion(), (unsigned)updater::percent());
  lv_label_set_text(updating_label, text);
}

void hideUpdating() {
  lv_screen_load(now_playing);
  lv_obj_delete(updating);
  updating = nullptr;
}

// While the display has no working Wi-Fi it offers its own network; this says how to join it.
void showSetup() {
  if (setup) return;
  if (picker) closePicker();
  const Theme& th = theme::active();
  setup = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(setup, th.background, 0);
  lv_obj_remove_flag(setup, LV_OBJ_FLAG_SCROLLABLE);

  // Phones join an open network from a "WIFI:" QR code with their camera.
  char join[64];
  snprintf(join, sizeof(join), "WIFI:T:nopass;S:%s;;", net::setupNetworkName());
  lv_obj_t* qr = lv_qrcode_create(setup);
  lv_qrcode_set_size(qr, 120);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, lv_color_white());
  lv_qrcode_update(qr, join, strlen(join));
  lv_obj_set_style_border_color(qr, lv_color_white(), 0);
  lv_obj_set_style_border_width(qr, 6, 0);  // the quiet zone scanners need
  lv_obj_align(qr, LV_ALIGN_LEFT_MID, 16, 0);

  lv_obj_t* text = lv_label_create(setup);
  lv_obj_set_width(text, 320 - 16 - 132 - 16 - 16);
  lv_obj_align(text, LV_ALIGN_RIGHT_MID, -16, 0);
  lv_obj_set_style_text_font(text, th.font_meta, 0);
  lv_obj_set_style_text_color(text, th.text, 0);
  lv_label_set_text_fmt(text,
                        "Set up Wi-Fi\n\nScan the code, or join \"%s\" in your phone's Wi-Fi settings. "
                        "A setup page opens.",
                        net::setupNetworkName());
  lv_screen_load(setup);
  backlight::fadeTo(backlight::kFull, kWakeFadeMs);
}

void hideSetup() {
  lv_screen_load(now_playing);
  lv_obj_delete(setup);
  setup = nullptr;
}

void checkIdle(lv_timer_t*) {
  if (updater::installing()) return showUpdating();
  if (updating) return hideUpdating();  // the install failed; the screensaver follows when idle
  bool needs_setup = net::inSetup() && net::wifi() != net::Wifi::kConnected;
  if (needs_setup) return showSetup();
  if (setup) return hideSetup();
  updater::setIdle(lv_screen_active() == screensaver);
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
