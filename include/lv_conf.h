// LVGL configuration. Anything not set here takes LVGL's default (lv_conf_internal.h).
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (32U * 1024U)  // peak use is ~11 KB; every KB here is a KB less heap for TLS

// Built-in LVGL display driver on top of TFT_eSPI.
#define LV_USE_TFT_ESPI 1

// Fonts used by the theme picker, dev screens and as the default. Unreferenced ones are dropped at link time.
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_LOG 0

// The Wi-Fi setup screen shows a QR code that joins the setup network.
#define LV_USE_QRCODE 1

// Widgets nothing uses. LVGL's default theme links in every enabled widget, so each costs flash
// (~35 KB together). Labels, images, buttons, bars and the canvas (under the QR code) stay.
#define LV_USE_ANIMIMG 0
#define LV_USE_ARC 0
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LED 0
#define LV_USE_LINE 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_MSGBOX 0
#define LV_USE_ROLLER 0
#define LV_USE_SCALE 0
#define LV_USE_SLIDER 0
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_SWITCH 0
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 0
#define LV_USE_TEXTAREA 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

#endif
