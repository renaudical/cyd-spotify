#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "net/timezones.h"
#include "themes/theme.h"
#include "ui.h"

namespace ui {
namespace {

void (*wake_cb)() = nullptr;

void onPressed(lv_event_t*) {
  // Swallow the rest of this touch so the wake tap can't press a button on the next screen.
  lv_indev_wait_release(lv_indev_active());
  wake_cb();
}

// Shows the time as "10:15 AM", or hides the label until NTP has set the clock and the zone is known.
void updateTime(lv_obj_t* label) {
  time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  if (local.tm_year < 2024 - 1900 || !tz::known()) {
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  int hour = local.tm_hour % 12 == 0 ? 12 : local.tm_hour % 12;
  char text[12];
  snprintf(text, sizeof(text), "%d:%02d %s", hour, local.tm_min, local.tm_hour < 12 ? "AM" : "PM");
  if (strcmp(text, lv_label_get_text(label)) != 0) lv_label_set_text(label, text);
  lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
}

void onTick(lv_timer_t* timer) { updateTime(static_cast<lv_obj_t*>(lv_timer_get_user_data(timer))); }

void onDelete(lv_event_t* e) { lv_timer_delete(static_cast<lv_timer_t*>(lv_event_get_user_data(e))); }

}  // namespace

lv_obj_t* createScreensaver(void (*on_wake)()) {
  wake_cb = on_wake;

  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* time_label = theme::active().build_screensaver(scr);
  if (time_label) {  // some themes have no clock
    lv_label_set_text(time_label, "");
    updateTime(time_label);

    // The screen outlives many screensaver visits, so the clock ticks whether or not it's showing.
    lv_timer_t* timer = lv_timer_create(onTick, 1000, time_label);
    lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, timer);
  }

  // Every touch must land on the screen itself, whatever the character art is made of.
  for (uint32_t i = 0; i < lv_obj_get_child_count(scr); i++) {
    lv_obj_remove_flag(lv_obj_get_child(scr, i), LV_OBJ_FLAG_CLICKABLE);
  }
  lv_obj_add_event_cb(scr, onPressed, LV_EVENT_PRESSED, nullptr);
  return scr;
}

// Dust: circles drifting and pulsing across a screen, in one or more strips that clip them. The
// album art screensaver has a strip either side of the art, which the player draws in the middle:
// each mote moves in screen coordinates and has a circle in each strip, so it slides behind the art
// and out the other side, and the clipping keeps LVGL from ever repainting the art, since the player
// draws it outside LVGL. The quiet screensaver has one strip, the whole screen.
namespace {

constexpr int kMaxMotes = 24;
constexpr int kArtMotes = 18;    // beside the album art
constexpr int kQuietMotes = 24;  // across the whole screen
constexpr uint32_t kFrameMs = 50;
constexpr float kMaxSpeed = 9;     // px/s
constexpr float kWander = 6;       // px/s^2 of random push
constexpr float kPulse = 0.3f;     // how much each mote grows and shrinks, as a fraction of its size

struct Mote {
  float x, y, vx, vy;  // screen coordinates, px/s
  float radius;
  float phase, pulse_rate;  // radians, radians/s
  lv_obj_t* circles[2];     // one per strip
};

struct Dust {
  int strip_count;
  lv_obj_t* strips[2];
  int32_t strip_x[2], strip_w;
  int32_t width, height;
  int mote_count;
  Mote motes[kMaxMotes];
  lv_timer_t* timer;
  uint32_t last_ms;
};

float between(float lo, float hi) { return lo + (hi - lo) * lv_rand(0, 10000) / 10000.0f; }

// Anywhere on the screen at first (some start behind the art); later just off a random edge.
void spawn(Dust& d, Mote& m, bool anywhere) {
  m.radius = between(2, 11);
  float margin = m.radius * (1 + kPulse) + 1;
  if (anywhere) {
    m.x = between(0, d.width);
    m.y = between(0, d.height);
  } else if (lv_rand(0, 1)) {
    m.x = between(0, d.width);
    m.y = lv_rand(0, 1) ? -margin : d.height + margin;
  } else {
    m.x = lv_rand(0, 1) ? -margin : d.width + margin;
    m.y = between(0, d.height);
  }
  // Drift in from wherever it starts.
  m.vx = between(-kMaxSpeed, kMaxSpeed) / 2 + (m.x < 0 ? 4 : m.x > d.width ? -4 : 0);
  m.vy = between(-kMaxSpeed, kMaxSpeed) / 2 + (m.y < 0 ? 4 : m.y > d.height ? -4 : 0);
  m.phase = between(0, 6.283f);
  m.pulse_rate = between(0.5f, 1.6f);
  lv_opa_t opa = lv_rand(LV_OPA_30, LV_OPA_80);
  for (int s = 0; s < d.strip_count; s++) lv_obj_set_style_bg_opa(m.circles[s], opa, 0);
}

void place(Dust& d, Mote& m) {
  float r = m.radius * (1 + kPulse * sinf(m.phase));
  int32_t size = std::max(2L, lroundf(2 * r));
  int32_t left = lroundf(m.x - r), top = lroundf(m.y - r);
  for (int s = 0; s < d.strip_count; s++) {
    lv_obj_t* c = m.circles[s];
    int32_t x0 = d.strip_x[s], x1 = x0 + d.strip_w;
    if (left + size <= x0 || left >= x1) {  // nowhere near this strip
      lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(c, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(c, left - x0, top);
    lv_obj_set_size(c, size, size);
  }
}

void onFrame(lv_timer_t* timer) {
  Dust& d = *static_cast<Dust*>(lv_timer_get_user_data(timer));
  uint32_t now = lv_tick_get();
  float dt = std::min<uint32_t>(now - d.last_ms, 200) / 1000.0f;
  d.last_ms = now;
  for (int i = 0; i < d.mote_count; i++) {
    Mote& m = d.motes[i];
    m.vx += between(-kWander, kWander) * dt;
    m.vy += between(-kWander, kWander) * dt;
    float speed = sqrtf(m.vx * m.vx + m.vy * m.vy);
    if (speed > kMaxSpeed) m.vx *= kMaxSpeed / speed, m.vy *= kMaxSpeed / speed;
    m.x += m.vx * dt;
    m.y += m.vy * dt;
    m.phase += m.pulse_rate * dt;
    float margin = m.radius * (1 + kPulse) + 2;
    if (m.x < -margin || m.x > d.width + margin || m.y < -margin || m.y > d.height + margin) spawn(d, m, false);
    place(d, m);
  }
}

// Only animates while it's showing.
void onLoaded(lv_event_t* e) {
  Dust& d = *static_cast<Dust*>(lv_event_get_user_data(e));
  d.last_ms = lv_tick_get();
  lv_timer_resume(d.timer);
}
void onUnloaded(lv_event_t* e) { lv_timer_pause(static_cast<Dust*>(lv_event_get_user_data(e))->timer); }
void onDeleteDust(lv_event_t* e) {
  Dust* d = static_cast<Dust*>(lv_event_get_user_data(e));
  lv_timer_delete(d->timer);
  delete d;
}

// White motes over `strip_count` strips: the two beside a full-height square in the middle, or the
// whole screen. They only animate while the screen is showing.
Dust* makeDust(lv_obj_t* scr, int strip_count, int mote_count) {
  Dust* d = new Dust();
  d->strip_count = strip_count;
  d->mote_count = mote_count;
  d->width = lv_display_get_horizontal_resolution(nullptr);
  d->height = lv_display_get_vertical_resolution(nullptr);
  int32_t strip_w = d->strip_w = strip_count == 2 ? (d->width - d->height) / 2 : d->width;
  for (int s = 0; s < strip_count; s++) {
    lv_obj_t* strip = lv_obj_create(scr);
    lv_obj_remove_style_all(strip);
    lv_obj_remove_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(strip, LV_OBJ_FLAG_CLICKABLE);
    d->strip_x[s] = s == 0 ? 0 : d->width - strip_w;
    lv_obj_set_pos(strip, d->strip_x[s], 0);
    lv_obj_set_size(strip, strip_w, d->height);
    d->strips[s] = strip;
  }
  for (int i = 0; i < mote_count; i++) {
    Mote& m = d->motes[i];
    for (int s = 0; s < strip_count; s++) {
      lv_obj_t* c = lv_obj_create(d->strips[s]);
      lv_obj_remove_style_all(c);
      lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(c, lv_color_white(), 0);
      m.circles[s] = c;
    }
    spawn(*d, m, true);
    place(*d, m);
  }

  d->timer = lv_timer_create(onFrame, kFrameMs, d);
  lv_timer_pause(d->timer);
  lv_obj_add_event_cb(scr, onLoaded, LV_EVENT_SCREEN_LOADED, d);
  lv_obj_add_event_cb(scr, onUnloaded, LV_EVENT_SCREEN_UNLOADED, d);
  lv_obj_add_event_cb(scr, onDeleteDust, LV_EVENT_DELETE, d);
  return d;
}

}  // namespace

lv_obj_t* createArtScreensaver(void (*on_wake)()) {
  wake_cb = on_wake;
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_user_data(scr, makeDust(scr, 2, kArtMotes));
  lv_obj_add_event_cb(scr, onPressed, LV_EVENT_PRESSED, nullptr);
  return scr;
}

void addDust(lv_obj_t* screen) { makeDust(screen, 1, kQuietMotes); }

void setArtScreensaverColors(lv_obj_t* screen, const lv_color_t colors[4]) {
  Dust& d = *static_cast<Dust*>(lv_obj_get_user_data(screen));
  for (int i = 0; i < d.mote_count; i++) {
    for (int s = 0; s < d.strip_count; s++) lv_obj_set_style_bg_color(d.motes[i].circles[s], colors[i % 4], 0);
  }
}

}  // namespace ui
