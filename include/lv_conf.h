// LVGL configuration. Anything not set here takes LVGL's default (lv_conf_internal.h).
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (48U * 1024U)

// Built-in LVGL display driver on top of TFT_eSPI.
#define LV_USE_TFT_ESPI 1

// Fonts referenced by themes and the calibration screen.
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_LOG 0

#endif
