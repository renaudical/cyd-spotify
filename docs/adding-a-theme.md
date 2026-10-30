# Adding a theme

Each theme is a character: colors and fonts for the Now Playing screen, plus its own clock screensaver. Themes are compiled into the firmware. Releasing a new version delivers the new theme to every display over Wi-Fi, where it shows up in the theme picker (hold the album art for 2 seconds). Nobody's display switches to it on its own.

## 1. Design it in Figma

Add two 320×240 frames to the design file (`VtOOfBfRpNR3xprHuxkHHp`), named like the existing ones:

- **Player - <Name>**: the Now Playing screen. The layout is shared by every theme. Only colors, fonts and letter spacing change, so start from a copy of "Player - Spider".
- **Clock - <Name>**: the screensaver. Anything goes, as long as there's a spot for the time ("10:15 AM", 12-hour).

Name your colors (e.g. "Gold", "SnowWhite"). Those names become the theme's tokens.

## 2. Write the theme file

Copy `src/themes/spider.h` and `spider.cpp` to `src/themes/<name>.h/.cpp` and rename the namespace.

- **Tokens** (`<name>.h`): one `constexpr` per Figma color, plus the fonts.
- **Roles** (`const Theme theme` in `<name>.cpp`): map tokens onto what each part of the screen uses. `src/themes/theme.h` says what each role is for.
- **Screensaver** (`buildScreensaver`): draws the clock face on an empty screen and returns the label the time goes in. The clock updates the label every second and keeps it hidden until it knows the time.
- **`name`**: the theme's permanent ID (lowercase, e.g. `"pumpkin"`). It's how a display remembers its choice, so never change it after a release. `label` is what the picker shows, and you can change it any time.

Then add the theme to the end of the list in `src/themes/themes.cpp` (and `#include` its header). The list order is the picker order.

## 3. Keep it within the flash budget

Every theme ships in every firmware, and the firmware must fit in a 1.875 MB slot. `pio run` prints the space used:

```
Flash: [========  ]  82.7% (used 1626033 bytes from 1966080 bytes)
```

Stay under about 95%. Past that there's no room left for fixes. What things cost:

| Element | Flash |
|---|---|
| Shapes drawn with LVGL (rectangles, rounded pills, borders; see `santa.cpp`) | almost nothing |
| A new clock font (only the glyphs `0-9 : A M P`) | 1–2 KB |
| A new text font size (full Latin character set) | 8–10 KB |
| One-color image (A8, recolored at runtime; see the icons), per pixel | 1 byte (320×240 = 77 KB) |
| Full-color image (RGB565), per pixel | 2 bytes (320×240 = 154 KB) |

So: draw with shapes when you can. Use a one-color image for a single-color illustration, and a full-color image only for the part that needs it, cropped to its size rather than full screen. Spider's full-screen web (154 KB) is the most expensive thing in the firmware.

Compressed or palette images (PNG, JPEG, LVGL's indexed and RLE formats) don't help here: LVGL unpacks them into RAM before drawing, and a full-screen image needs about 150 KB of RAM, while the display has under 100 KB free. So use RGB565 or A8, which draw straight from flash.

If the budget runs out, retire an old theme by removing it from `themes.cpp` and deleting its files. Displays that were using it fall back to the first theme in the list.

## 4. Generate fonts and images

Fonts and images become C files through `tools/gen_assets.sh`:

- **Fonts:** add a `font` line. Grift files are in `design/fonts/`; pass `"$clock_range"` for a clock-only font. Then declare the font in `src/fonts/fonts.h`.
- **Images:** export from Figma at 1× (or larger; the script resizes), put the PNG in `design/assets/`, and add a `png_to_lvgl.py` line using `rgb565` or `a8`. Then declare the image in `src/assets/assets.h`.

Run `tools/gen_assets.sh` (needs node, `rsvg-convert` from librsvg, and macOS `sips`). Don't edit the generated files by hand.

## 5. Try it

Flash a build with the fake playlist, so you can see long titles, accents, missing art and paused or stopped states without music playing:

```sh
PLATFORMIO_BUILD_FLAGS=-DFAKE_PLAYER pio run -t upload
```

Hold the album art, pick the new theme and compare both screens with Figma. The screensaver starts after 10 seconds without a touch. Then flash a normal build (`pio run -t upload`) and check it with real music.

## 6. Release it

Commit, push and tag a new version. Themes are new features, so bump the middle number:

```sh
git tag v1.3.0 && git push origin v1.3.0
```

The release workflow builds the firmware, publishes a GitHub release and updates the web installer. Displays check for updates every 6 hours while the screensaver is showing, install the new version and restart. It then appears in their theme pickers.
