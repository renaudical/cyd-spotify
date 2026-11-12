#pragma once

#include <lvgl.h>

// The app UI: a Now Playing screen and a screensaver (plus a dark sleep screen), kept alive and rebuilt when the theme changes,
// plus a theme picker opened by holding the album art.
namespace ui {

// Creates both screens, shows Now Playing, and starts the idle timer.
void begin();

// `on_hold` runs when the album art is held down for a couple of seconds; `on_sleep` when the moon
// in the top-right corner is tapped.
lv_obj_t* createNowPlaying(void (*on_hold)(), void (*on_sleep)());
// `on_wake` runs when the screensaver is touched; that touch never reaches the next screen.
lv_obj_t* createScreensaver(void (*on_wake)());
// The album art screensaver: black (or, with `backdrop`, the art blurred), with dust drifting either
// side of the art, which the player draws.
lv_obj_t* createArtScreensaver(void (*on_wake)(), bool backdrop);
// Colors the dust (four colors from the art) and, if it has one, blurs the art into the backdrop.
void setArtScreensaverArt(lv_obj_t* screen, const lv_color_t colors[4], const lv_image_dsc_t* art);
// White dust drifting across the whole of a screen, for a theme's build_screensaver (it animates only
// while the screen shows).
void addDust(lv_obj_t* screen);
// Lists the themes. Both callbacks run outside the screen's event handlers, so they may delete it.
lv_obj_t* createThemePicker(void (*on_pick)(size_t index), void (*on_close)());

}  // namespace ui
