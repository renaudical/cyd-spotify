#include "ui.h"

#include <Arduino.h>
#include <initializer_list>
#include <stdio.h>

#include "backlight.h"
#include "leds/leds.h"
#include "net/net.h"
#include "palette.h"
#include "player/player.h"
#include "settings/settings.h"
#include "themes/theme.h"
#include "updater/updater.h"

namespace ui {
namespace {

constexpr uint32_t kIdleTimeoutMs = 10000;
constexpr uint8_t kScreensaverBrightness = 64;  // 25% backlight power
constexpr uint8_t kArtScreensaverBrightness = 128;  // album art looks muddy any dimmer
constexpr uint32_t kDimFadeMs = 800;
constexpr uint32_t kWakeFadeMs = 150;
// Between tracks the art is gone for a second or two while the next one downloads; the old art stays
// up meanwhile instead of flashing the clock. The same grace applies to pausing, and to the LEDs.
constexpr uint32_t kArtGapMs = 8000;

lv_obj_t* now_playing = nullptr;
lv_obj_t* screensaver = nullptr;
lv_obj_t* art_screensaver = nullptr;  // only when it's the setting or the theme's; the player draws the art
bool art_screensaver_setting = false;
uint32_t art_missing_since = 0;  // millis() when the album art screensaver lost its art or music; 0 while it has them
uint32_t quiet_since = 0;  // millis() when music stopped playing, for the LEDs; 0 while it plays
uint32_t led_colors_version = 0;  // the State::version the LEDs last took their colors from
bool led_colors_fresh = false;
lv_obj_t* asleep = nullptr;  // blank, with the backlight off, until touched
lv_obj_t* picker = nullptr;  // only exists while open
lv_obj_t* setup = nullptr;  // only exists while the setup network is up
lv_obj_t* updating = nullptr;  // only exists while an update installs
lv_obj_t* updating_label = nullptr;

void openPicker();

void wake() {
  player::showFullScreenArt(false);
  lv_screen_load(now_playing);
  backlight::fadeTo(backlight::kFull, kWakeFadeMs);
}

// Turns the screen off until it's touched; that touch never reaches Now Playing.
void sleep() {
  if (!asleep) {
    asleep = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(asleep, lv_color_black(), 0);
    lv_obj_remove_flag(asleep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(
        asleep,
        [](lv_event_t*) {
          lv_indev_wait_release(lv_indev_active());
          wake();
        },
        LV_EVENT_PRESSED, nullptr);
  }
  lv_screen_load(asleep);
  backlight::fadeTo(0, kDimFadeMs);
}

void logPool() {
  lv_mem_monitor_t mem;
  lv_mem_monitor(&mem);
  Serial.printf("ui: LVGL pool %u of %u bytes used\n", (unsigned)(mem.total_size - mem.free_size), (unsigned)mem.total_size);
}

void buildScreens() {
  now_playing = createNowPlaying(openPicker, sleep);
  screensaver = createScreensaver(wake);
  bool art = art_screensaver_setting || theme::active().art_screensaver;
  art_screensaver = art ? createArtScreensaver(wake) : nullptr;
  leds::setEnabled(theme::active().leds);
  led_colors_fresh = false;
}

// Colors the dust beside the art with the art's own colors.
void paintArtColors(const lv_image_dsc_t* art) {
  palette::Rgb rgb[palette::kColors];
  palette::fromArt(reinterpret_cast<const uint16_t*>(art->data), art->header.w, art->header.h, rgb);
  lv_color_t colors[palette::kColors];
  for (int i = 0; i < palette::kColors; i++) colors[i] = lv_color_make(rgb[i].r, rgb[i].g, rgb[i].b);
  setArtScreensaverColors(art_screensaver, colors);
}

bool playingArt(const player::State& s) { return s.art && s.status == player::Status::kPlaying; }

// Album art while something plays (when that's the setting), the theme's own screensaver otherwise.
lv_obj_t* screensaverFor(const player::State& s) {
  return art_screensaver && playingArt(s) ? art_screensaver : screensaver;
}

// The LED Lights theme's strand: the album's colors while music plays, white twinkling otherwise.
void feedLeds(const player::State& s) {
  if (!theme::active().leds) return;
  bool playing = s.link == player::Link::kOk && s.status == player::Status::kPlaying;
  if (playing) quiet_since = 0;
  else if (!quiet_since) quiet_since = millis() | 1;
  leds::setPlaying(playing || millis() - quiet_since < kArtGapMs);
  if (led_colors_fresh && s.version == led_colors_version) return;
  led_colors_fresh = true;
  led_colors_version = s.version;
  if (!s.art) return leds::setColors(nullptr);
  palette::Rgb rgb[palette::kColors];
  palette::fromArt(reinterpret_cast<const uint16_t*>(s.art->data), s.art->header.w, s.art->header.h, rgb);
  leds::setColors(rgb);
}

void showScreensaver(lv_obj_t* scr) {
  bool art = scr == art_screensaver;
  art_missing_since = 0;
  player::showFullScreenArt(false);
  if (art) paintArtColors(player::state().art);
  lv_screen_load(scr);
  if (art) {
    lv_refr_now(nullptr);  // the player draws over what LVGL has drawn, so LVGL goes first
    player::showFullScreenArt(true);
  }
  backlight::fadeTo(art ? kArtScreensaverBrightness : kScreensaverBrightness, kDimFadeMs);
}

void closePicker() {
  lv_screen_load(now_playing);
  lv_obj_delete(picker);
  picker = nullptr;
}

// Screens take their colors when they're built, so a new theme means building them again. The old
// ones go first: two sets of dust don't fit in LVGL's pool at once. The picker is showing, so it goes
// last.
void pickTheme(size_t index) {
  for (lv_obj_t* old : {now_playing, screensaver, art_screensaver}) {
    if (old) lv_obj_delete(old);
  }
  theme::select(index);
  buildScreens();
  lv_screen_load(now_playing);
  lv_obj_delete(picker);
  picker = nullptr;
  logPool();
}

void openPicker() {
  picker = createThemePicker(pickTheme, closePicker);
  lv_screen_load(picker);
}

// Takes over the display while an update downloads; the device restarts when it's done.
void showUpdating() {
  if (!updating) {
    player::showFullScreenArt(false);
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
  player::showFullScreenArt(false);
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
  feedLeds(player::state());
  if (updater::installing()) return showUpdating();
  if (updating) return hideUpdating();  // the install failed; the screensaver follows when idle
  bool needs_setup = net::inSetup() && net::wifi() != net::Wifi::kConnected;
  if (needs_setup) return showSetup();
  if (setup) return hideSetup();
  lv_obj_t* active = lv_screen_active();
  bool saving = active == screensaver || active == art_screensaver;
  updater::setIdle(saving);
  // Music starting or stopping swaps the clock and the album art.
  const player::State& s = player::state();
  if (active == art_screensaver) {
    if (playingArt(s) && art_missing_since) paintArtColors(s.art);  // the player draws the new art itself
    if (playingArt(s)) art_missing_since = 0;
    else if (!art_missing_since) art_missing_since = millis() | 1;
    else if (millis() - art_missing_since > kArtGapMs) return showScreensaver(screensaver);
  } else if (active == screensaver && screensaverFor(s) == art_screensaver) {
    return showScreensaver(art_screensaver);
  }
  if (lv_display_get_inactive_time(nullptr) < kIdleTimeoutMs) return;
  if (picker) closePicker();  // the screensaver follows on the next check
  else if (active == now_playing) showScreensaver(screensaverFor(player::state()));
}

}  // namespace

void begin() {
  art_screensaver_setting = settings::artScreensaver();
  buildScreens();
  lv_screen_load(now_playing);
  lv_timer_create(checkIdle, 250, nullptr);
  logPool();
}

}  // namespace ui
