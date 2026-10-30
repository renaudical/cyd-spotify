#include <string.h>

#include "assets/assets.h"
#include "player/player.h"
#include "themes/theme.h"
#include "ui.h"

namespace ui {
namespace {

// Geometry from the Figma "Player" frame (320x240).
constexpr int32_t kArtX = 16, kArtY = 24, kArtSize = 100;
constexpr int32_t kTextX = 128, kTextY = 24, kTextW = 172, kTextGap = 12;
constexpr int32_t kBarX = 12, kBarY = 146, kBarW = 296, kBarH = 4;
constexpr int32_t kButtonY = 172, kButtonSize = 44;
constexpr int32_t kButtonX[] = {24, 138, 252};
constexpr int32_t kBarRange = 1000;

struct NowPlayingUi {
  lv_obj_t* art;
  lv_obj_t* art_placeholder;
  lv_obj_t* title;
  lv_obj_t* artist;
  lv_obj_t* album;
  lv_obj_t* progress;
  uint32_t shown_version;
};

NowPlayingUi ui_ = {};

constexpr uint32_t kHoldMs = 2000;
void (*hold_cb)() = nullptr;

constexpr char kEllipsis[] = "\xe2\x80\xa6";  // U+2026

lv_obj_t* makeText(lv_obj_t* parent, const lv_font_t* font, int32_t letter_space, int max_lines) {
  const Theme& th = theme::active();
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_width(label, kTextW);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, th.text, 0);
  lv_obj_set_style_text_letter_space(label, letter_space, 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_user_data(label, reinterpret_cast<void*>(max_lines));
  return label;
}

bool fits(lv_obj_t* label, const char* text, int max_lines) {
  const lv_font_t* font = lv_obj_get_style_text_font(label, 0);
  lv_point_t size;
  lv_text_get_size(&size, text, font, lv_obj_get_style_text_letter_space(label, 0), 0, kTextW, LV_TEXT_FLAG_NONE);
  return size.y <= max_lines * lv_font_get_line_height(font);
}

// Sets the label text, cut at the last whole word that fits the label's line limit, plus "…".
// LVGL's own LONG_DOT mode cuts mid-word and relies on a fixed height, which clips lines.
void setTruncated(lv_obj_t* label, const char* text) {
  int max_lines = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(label)));
  if (fits(label, text, max_lines)) {
    lv_label_set_text(label, text);
    return;
  }

  static char buf[sizeof(player::State::title) + sizeof(kEllipsis)];
  size_t len = strlen(text);
  if (len >= sizeof(player::State::title)) len = sizeof(player::State::title) - 1;

  // Longest prefix (on a UTF-8 boundary) that fits with the ellipsis appended.
  size_t lo = 0, hi = len;
  while (lo < hi) {
    size_t mid = (lo + hi + 1) / 2;
    while (mid > 0 && (text[mid] & 0xC0) == 0x80) mid--;
    if (mid <= lo) {
      hi = lo;
      break;
    }
    memcpy(buf, text, mid);
    strcpy(buf + mid, kEllipsis);
    if (fits(label, buf, max_lines)) lo = mid;
    else hi = mid - 1;
  }
  size_t cut = lo;
  while (cut > 0 && (text[cut] & 0xC0) == 0x80) cut--;

  // Back up to a word boundary unless that would throw away most of the last line.
  size_t word = cut;
  while (word > 0 && text[word] != ' ') word--;
  if (word > cut * 3 / 4) cut = word;
  // Don't leave dangling spaces or separators before the ellipsis.
  while (cut > 0 && strchr(" -,:;([&", text[cut - 1])) cut--;

  memcpy(buf, text, cut);
  strcpy(buf + cut, kEllipsis);
  lv_label_set_text(label, buf);
}

void makeButton(lv_obj_t* parent, int32_t x, const lv_image_dsc_t* icon, void (*action)()) {
  const Theme& th = theme::active();
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_remove_style_all(button);
  lv_obj_set_pos(button, x, kButtonY);
  lv_obj_set_size(button, kButtonSize, kButtonSize);
  lv_obj_set_style_radius(button, 4, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, th.button_border, 0);
  lv_obj_set_style_border_opa(button, th.button_border_opa, 0);
  lv_obj_set_style_bg_color(button, th.button_pressed, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, th.button_pressed_opa, LV_STATE_PRESSED);
  lv_obj_add_event_cb(
      button, [](lv_event_t* e) { reinterpret_cast<void (*)()>(lv_event_get_user_data(e))(); }, LV_EVENT_CLICKED,
      reinterpret_cast<void*>(action));

  lv_obj_t* image = lv_image_create(button);
  lv_image_set_src(image, icon);
  lv_obj_set_style_image_recolor(image, th.icon, 0);
  lv_obj_set_style_image_recolor_opa(image, LV_OPA_COVER, 0);
  lv_obj_center(image);
}

void showMessage(const char* title, const char* detail) {
  lv_label_set_text(ui_.title, title);
  lv_label_set_text(ui_.artist, detail);
  lv_label_set_text(ui_.album, "");
  lv_obj_add_flag(ui_.art, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(ui_.art_placeholder, LV_OBJ_FLAG_HIDDEN);
}

void showTrack(const player::State& s) {
  switch (s.link) {
    case player::Link::kOk:
      break;
    case player::Link::kWifiConnecting:
      showMessage("Connecting to Wi-Fi\xe2\x80\xa6", "");
      return;
    case player::Link::kWifiLost:
      showMessage("Wi-Fi connection lost", "Reconnecting\xe2\x80\xa6");
      return;
    case player::Link::kWiimConnecting:
      showMessage("Connecting to WiiM\xe2\x80\xa6", "");
      return;
    case player::Link::kWiimUnreachable:
      showMessage("Can\xe2\x80\x99t reach the WiiM", "Retrying\xe2\x80\xa6");
      return;
    case player::Link::kWiimNotFound:
      showMessage("No WiiM found on this network", "Still looking\xe2\x80\xa6");
      return;
  }

  if (s.status == player::Status::kStopped) {
    lv_label_set_text(ui_.title, "Nothing playing");
    lv_label_set_text(ui_.artist, "");
    lv_label_set_text(ui_.album, "");
  } else {
    setTruncated(ui_.title, s.title);
    setTruncated(ui_.artist, s.artist);
    setTruncated(ui_.album, s.album);
  }

  if (s.art) {
    // The player reuses its art buffers, so the same descriptor can come back with new pixels.
    lv_image_cache_drop(s.art);
    lv_image_set_src(ui_.art, s.art);
    lv_obj_invalidate(ui_.art);
    lv_obj_remove_flag(ui_.art, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_.art_placeholder, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ui_.art, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(ui_.art_placeholder, LV_OBJ_FLAG_HIDDEN);
  }
}

void refresh(lv_timer_t*) {
  const player::State& s = player::state();
  if (s.version != ui_.shown_version) {
    showTrack(s);
    ui_.shown_version = s.version;
  }
  bool show_progress = s.link == player::Link::kOk && s.duration_ms;
  int32_t value = show_progress ? (int64_t)s.position_ms * kBarRange / s.duration_ms : 0;
  lv_bar_set_value(ui_.progress, LV_MIN(value, kBarRange), LV_ANIM_OFF);
}

// Holding the art for kHoldMs runs hold_cb once; the rest of that touch is swallowed.
void onArtTouch(lv_event_t* e) {
  static uint32_t pressed_at = 0;
  static bool fired = false;
  if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
    pressed_at = lv_tick_get();
    fired = false;
  } else if (!fired && lv_tick_elaps(pressed_at) >= kHoldMs) {
    fired = true;
    lv_indev_wait_release(lv_indev_active());
    hold_cb();
  }
}

}  // namespace

lv_obj_t* createNowPlaying(void (*on_hold)()) {
  hold_cb = on_hold;
  const Theme& th = theme::active();

  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, th.background, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Art box: the placeholder fills it; the art image covers the placeholder when present.
  lv_obj_t* art_box = lv_obj_create(scr);
  lv_obj_remove_style_all(art_box);
  lv_obj_set_pos(art_box, kArtX, kArtY);
  lv_obj_set_size(art_box, kArtSize, kArtSize);
  lv_obj_set_style_bg_color(art_box, th.art_placeholder_bg, 0);
  lv_obj_set_style_bg_opa(art_box, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(art_box, onArtTouch, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(art_box, onArtTouch, LV_EVENT_PRESSING, nullptr);

  ui_.art_placeholder = lv_label_create(art_box);
  lv_obj_set_style_text_font(ui_.art_placeholder, th.font_art_placeholder, 0);
  lv_obj_set_style_text_color(ui_.art_placeholder, th.art_placeholder_fg, 0);
  lv_label_set_text(ui_.art_placeholder, LV_SYMBOL_AUDIO);
  lv_obj_center(ui_.art_placeholder);

  ui_.art = lv_image_create(art_box);
  lv_obj_set_size(ui_.art, kArtSize, kArtSize);

  // Title, artist, album stacked from the top, as in Figma.
  lv_obj_t* text = lv_obj_create(scr);
  lv_obj_remove_style_all(text);
  lv_obj_set_pos(text, kTextX, kTextY);
  lv_obj_set_size(text, kTextW, kArtSize);
  lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(text, kTextGap, 0);
  lv_obj_remove_flag(text, LV_OBJ_FLAG_CLICKABLE);
  ui_.title = makeText(text, th.font_title, 0, 2);
  ui_.artist = makeText(text, th.font_meta, th.meta_letter_space, 1);
  ui_.album = makeText(text, th.font_meta, th.meta_letter_space, 2);

  ui_.progress = lv_bar_create(scr);
  lv_obj_remove_style_all(ui_.progress);
  lv_obj_set_pos(ui_.progress, kBarX, kBarY);
  lv_obj_set_size(ui_.progress, kBarW, kBarH);
  lv_obj_set_style_bg_color(ui_.progress, th.progress_track, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_.progress, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_.progress, th.progress_fill, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(ui_.progress, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(ui_.progress, kBarH, LV_PART_INDICATOR);
  lv_bar_set_range(ui_.progress, 0, kBarRange);

  makeButton(scr, kButtonX[0], &icon_prev, player::previous);
  makeButton(scr, kButtonX[1], &icon_play_pause, player::togglePlayPause);
  makeButton(scr, kButtonX[2], &icon_next, player::next);

  ui_.shown_version = player::state().version - 1;
  refresh(nullptr);
  // The screen is deleted and rebuilt when the theme changes; its timer goes with it.
  lv_timer_t* timer = lv_timer_create(refresh, 100, nullptr);
  lv_obj_add_event_cb(
      scr, [](lv_event_t* e) { lv_timer_delete(static_cast<lv_timer_t*>(lv_event_get_user_data(e))); },
      LV_EVENT_DELETE, timer);
  return scr;
}

}  // namespace ui
