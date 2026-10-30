# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**Now Playing Brickman**: firmware for the ESP32-2432S028R ("Cheap Yellow Display"): ILI9341 TFT used at 320×240 landscape, XPT2046 resistive touch. PlatformIO + Arduino framework + LVGL 9. It's a "now playing" display and remote for a WiiM Mini, built in stages: (1) UI with fake data, (2) live data over the WiiM/LinkPlay HTTP API, (3) album art, (4) working controls. Designs are in Figma (file `VtOOfBfRpNR3xprHuxkHHp`, frames "Player - Holiday", "Player - Spider", "Clock - Holiday", "Clock - Spider"; the older "Screen - *" frames are the retired screensavers); the PNGs in `design/assets/` are older than Figma.

## Commands

The single environment is `cyd`. The board shows up as a CH340 serial port (`/dev/cu.usbserial-*`); PlatformIO auto-detects it.

- Build: `pio run`
- Flash: `pio run -t upload`
- Serial monitor: `pio device monitor` (115200)
- Flash and monitor: `pio run -t upload -t monitor`
- Tests: `pio test`; a single suite: `pio test -f <test_dir_name>` (no tests exist yet)
- Hardware test screens (color swatches, touch test, calibration grid) instead of the app: `PLATFORMIO_BUILD_FLAGS=-DDEV_SCREENS pio run -t upload`
- App with the fake playlist instead of the WiiM (UI work without a WiiM): `PLATFORMIO_BUILD_FLAGS=-DFAKE_PLAYER pio run -t upload`
- Serial output without a terminal: `pio device monitor` needs an interactive stdin, so read the port with pyserial instead (PlatformIO's own Python has it).
- Regenerate fonts, icons and demo art: `tools/gen_assets.sh` (needs node, `rsvg-convert` from librsvg, macOS `sips`)
- Regenerate the timezone table (after tz database updates): `python3 -I tools/gen_timezones.py > src/net/tz_table.cpp`

UI font is Grift (royalty-free; `design/fonts/Grift-*.otf`). The old Hoefler&Co Ringside files in `design/fonts/` are licensed and gitignored; don't use them.

Partition table is `huge_app.csv` (3 MB app, ~900 KB filesystem, no OTA); always flash over USB.

Toolchain pins matter: `espressif32 @ 6.x` is Arduino core 2.0.x, which TFT_eSPI 2.5.x works with. Moving to platform 7+/Arduino 3.x is a known break risk for TFT_eSPI. The XPT2046 library comes from a git tag because the registry copy is a stale 2019 alpha without `begin(SPIClass&)`.

## Hardware: display and touch are on separate SPI buses

This is the main trap on this board. The display uses HSPI and touch uses VSPI, with different pins. Anything that assumes touch shares the display bus (e.g. TFT_eSPI's built-in touch) won't work. `src/touch.cpp` drives the XPT2046 on its own `SPIClass(VSPI)`.

| Function | Display (ILI9341, HSPI) | Touch (XPT2046, VSPI) |
|---|---|---|
| SCK  | 14 | 25 |
| MOSI | 13 | 32 |
| MISO | 12 | 39 |
| CS   | 15 | 33 |
| Other | DC 2, backlight 21, no RST | IRQ 36 |

Display pins are TFT_eSPI build flags in `platformio.ini` (`USER_SETUP_LOADED`; never edit the library's `User_Setup.h`). Touch pins and calibration live in `src/board.h`. Touches within `kTouchDeadZone` (12px) of any edge are dropped: the bezel presses the resistive panel there (~3px in) and otherwise causes a permanent phantom touch, which also stops the screensaver from ever starting.

## Architecture

- **Orientation:** the panel is natively 240×320 portrait. `main.cpp` creates the LVGL display at native size and calls `lv_display_set_rotation(..., 90)` to get 320×240 landscape. The touch read callback reports points in *native portrait* coordinates (raw values mapped through `kTouchRaw*` in `board.h`, XPT2046 rotation 2), and LVGL rotates them to match the display. If touches land mirrored or offset, fix the mapping or rotation in `touch.cpp`/`board.h`. Don't swap axes in the screens.
- **Themes:** `src/themes/theme.h` defines a `Theme` struct of semantic roles (background, text, progress, button, art placeholder, fonts…) plus a `build_screensaver` function, so each character brings its own clock screensaver (Santa draws shapes; Spider shows a full-screen RGB565 image) and returns the label the time goes in. Each character gets its own file of named tokens (`santa.h`, `spider.h`) plus a `Theme` mapping them onto roles. `themes.cpp` lists every theme in picker order; adding one means a new file plus a line there. The choice is saved by `name` in NVS (`Preferences`, namespace `now-playing`). Screens use only `theme::active()` roles, never character tokens or raw colors.
- **Theme picker:** holding the album art for 2 s opens `theme_picker_screen.cpp`. Screens take their colors when built, so picking a theme builds new Now Playing and screensaver screens and deletes the old ones (Now Playing deletes its refresh timer on `LV_EVENT_DELETE`). The picker is deleted when closed, and closes itself after the idle timeout.
- **Player interface:** `src/player/player.h` is the only thing the UI knows about playback: a `State` (with a `version` bumped on any non-position change) and `togglePlayPause`/`next`/`previous`. `wiim_player.cpp` implements it: a task on core 0 polls `getPlayerStatus` every second and hands snapshots (under a mutex) to `player::update()` on the UI core, which interpolates the position between polls. `Link` covers Wi-Fi connecting/lost and WiiM connecting/unreachable (3 failed polls). `fake_player.cpp` (`-DFAKE_PLAYER`) is a playlist of edge cases (long text, no art, accents, nothing playing).
- **WiiM API facts (checked on the real WiiM Mini):** HTTPS only (port 80 closed), self-signed LinkPlay certificate, so `WiFiClientSecure::setInsecure()`; keep-alive works (`HTTPClient::setReuse`). `getPlayerStatus` has Spotify metadata: `Title`/`Artist`/`Album` are hex-encoded UTF-8 containing HTML entities (`&quot;`), `curpos`/`totlen` in ms, `status` play/pause/stop/load. Every text field is cut to 64 bytes, mid-word or mid-character, and UPnP returns the same cut, so there's no fuller source; `wiim_text.cpp` decodes fields and turns a cut into a whole word plus "…". `getMetaInfo` has `albumArtURI` (Spotify CDN; the 640px URL is rewritten to the 300px one). Controls are `setPlayerCmd:onepause|next|prev`; the UI queues them to the network task, which polls right after sending. The host is `WIIM_HOST` in `include/secrets.h`; the WiiM advertises `_linkplay._tcp` over mDNS.
- **App screens:** `ui::begin()` creates Now Playing and the screensaver once and keeps both alive. Now Playing polls `player::state()` on an LVGL timer and only redraws text/art when `version` changes. After 10 s of no touch (`lv_display_get_inactive_time`) the screensaver loads and the backlight dims (PWM on GPIO 21, `backlight.cpp`). A touch calls `lv_indev_wait_release` before waking, so the wake tap can't hit a button.
- **Clock:** the screensaver shows 12-hour time ("10:15 AM"). NTP gives only UTC; `src/net/timezones.cpp` (`tz::poll()`, run by the net task while Wi-Fi is up) starts SNTP and sets the zone: `TIMEZONE` in `secrets.h` (POSIX TZ string) if defined, otherwise the zone saved in NVS (key `tz`) and then, once per boot, an IP geolocation lookup (`ip-api.com` over plain HTTP, so no TLS heap; retries with backoff; wrong behind a VPN). The IANA name it returns maps to a POSIX string via `tz_table.cpp`, generated from the Mac's `/usr/share/zoneinfo` by `python3 -I tools/gen_timezones.py > src/net/tz_table.cpp`. `screensaver_screen.cpp` updates the label on a 1 s LVGL timer (deleted on `LV_EVENT_DELETE`) and hides it until the clock is set and `tz::known()`, so a fresh board never shows UTC. The clock fonts (`grift_semibold_26`/`_21`) contain only `0-9 : A M P`, so their line height is cap height, not Figma's; theme files offset/pad the label to match the frames.
- **Layout:** positions in `now_playing_screen.cpp` are copied from the Figma "Player" frame. Title/artist/album are cut to 2/1/2 lines by `setTruncated()` (measures with the real font, cuts at a word, appends U+2026); don't switch them to LVGL's `LV_LABEL_LONG_DOT`. Album art is a fixed 100×100 box: images must arrive already scaled to it, and a themed placeholder shows when `State::art` is null.
- **Album art and RAM:** `album_art.cpp` downloads to LittleFS (`/art.jpg`), then JPEGDEC decodes at 1/2–1/8 scale and the result is cropped and averaged to 100×100 *while decoding*, one band of rows at a time. RAM is the constraint: static RAM (`dram0_0_seg`) is nearly full, so big buffers must be heap-allocated (the two 20 KB art buffers in `wiim_player.cpp`), and the largest free heap block is only ~43 KB. Two TLS sessions don't fit at once, so the WiiM connection is closed before an art download. Don't hold a whole JPEG or a full decoded image in RAM.
- **Dev screens:** `src/ui/` color swatches (boot screen; buttons link it with the touch test), touch test and calibration grid, compiled only with `DEV_SCREENS`. They rebuild their screen on every `show*()` and free it on `LV_EVENT_DELETE`, so switching from inside an event handler must go through `lv_async_call`.
- **Generated assets:** `src/assets/*.c` (icons as A8, recolored at runtime; demo art as RGB565) and `src/fonts/*.c` come from `tools/gen_assets.sh`. Don't edit them by hand.
- **LVGL config:** `include/lv_conf.h` is minimal; everything else uses LVGL's defaults. Fonts must be enabled there (`LV_FONT_MONTSERRAT_*`) before a theme can reference them.
