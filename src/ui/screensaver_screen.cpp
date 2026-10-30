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
  lv_label_set_text(time_label, "");
  updateTime(time_label);

  // The screen outlives many screensaver visits, so the clock ticks whether or not it's showing.
  lv_timer_t* timer = lv_timer_create(onTick, 1000, time_label);
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, timer);

  // Every touch must land on the screen itself, whatever the character art is made of.
  for (uint32_t i = 0; i < lv_obj_get_child_count(scr); i++) {
    lv_obj_remove_flag(lv_obj_get_child(scr, i), LV_OBJ_FLAG_CLICKABLE);
  }
  lv_obj_add_event_cb(scr, onPressed, LV_EVENT_PRESSED, nullptr);
  return scr;
}

}  // namespace ui
