#!/usr/bin/env bash
# Regenerates src/fonts/ and src/assets/ from design/ and img/.
# Needs node (npx), rsvg-convert (brew install librsvg) and sips (macOS).
set -euo pipefail
cd "$(dirname "$0")/.."

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p src/fonts src/assets

# ASCII, Latin-1, dashes, curly quotes, ellipsis: enough for most track metadata.
range=0x20-0x7E,0xA0-0xFF,0x2013-0x2014,0x2018-0x2019,0x201C-0x201D,0x2026

# Clock faces only ever show "12:34 AM"/"PM", so their fonts carry just those glyphs.
clock_range=0x20,0x30-0x3A,0x41,0x4D,0x50

font() {  # <otf file> <px size> <symbol name> [glyph range]
  npx -y lv_font_conv@1.5.2 --no-compress --format lvgl --bpp 4 --size "$2" \
    --font "design/fonts/$1" -r "${4:-$range}" --lv-include lvgl.h -o "src/fonts/$3.c"
}
font Grift-SemiBold.otf 14 grift_semibold_14
font Grift-Medium.otf 12 grift_medium_12
font Grift-SemiBold.otf 26 grift_semibold_26 "$clock_range"
font Grift-SemiBold.otf 21 grift_semibold_21 "$clock_range"

# The art placeholder's music note (LV_SYMBOL_AUDIO) alone, from the FontAwesome subset LVGL ships;
# all of Montserrat 28 just for this glyph cost 35 KB of flash.
npx -y lv_font_conv@1.5.2 --no-compress --format lvgl --bpp 4 --size 28 \
  --font .pio/libdeps/cyd/lvgl/scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff -r 0xF001 \
  --lv-include lvgl.h -o src/fonts/symbol_audio_28.c

for icon in prev play_pause next; do
  rsvg-convert -w 32 -h 32 "design/assets/icons/$icon.svg" -o "$tmp/$icon.png"
  python3 -I tools/png_to_lvgl.py "$tmp/$icon.png" a8 "icon_$icon" > "src/assets/icon_$icon.c"
done

# Stage-1 demo album art, pre-scaled to the 100x100 art box.
sips -s format png -z 100 100 img/guaraldi.jpeg --out "$tmp/guaraldi.png" > /dev/null
python3 -I tools/png_to_lvgl.py "$tmp/guaraldi.png" rgb565 demo_art_guaraldi > src/assets/demo_art_guaraldi.c

# Spider clock screensaver art, full screen (320x240 RGB565).
sips -s format png -z 240 320 design/assets/spider_screensaver.png --out "$tmp/spider_screensaver.png" > /dev/null
python3 -I tools/png_to_lvgl.py "$tmp/spider_screensaver.png" rgb565 spider_screensaver > src/assets/spider_screensaver.c

echo "Generated fonts and assets."
