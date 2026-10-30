#pragma once

#include <lvgl.h>

// The app UI: a Now Playing screen and a screensaver, kept alive and rebuilt when the theme changes,
// plus a theme picker opened by holding the album art.
namespace ui {

// Creates both screens, shows Now Playing, and starts the idle timer.
void begin();

// `on_hold` runs when the album art is held down for a couple of seconds.
lv_obj_t* createNowPlaying(void (*on_hold)());
// `on_wake` runs when the screensaver is touched; that touch never reaches the next screen.
lv_obj_t* createScreensaver(void (*on_wake)());
// Lists the themes. Both callbacks run outside the screen's event handlers, so they may delete it.
lv_obj_t* createThemePicker(void (*on_pick)(size_t index), void (*on_close)());

}  // namespace ui
