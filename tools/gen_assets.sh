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

# Everything else track metadata often has: Central European, Vietnamese, Greek, Cyrillic, Japanese kana,
# symbols like ★ and ♪ (WANTED in tools/subset_noto.py). Grift has some of it; the Noto fonts made by
# that script have the rest and become each text font's fallback. Kanji, Chinese and Korean don't fit
# in flash; LVGL draws a box for them.
source tools/font_ranges.sh

# Clock faces only ever show "12:34 AM"/"PM", so their fonts carry just those glyphs.
clock_range=0x20,0x30-0x3A,0x41,0x4D,0x50

font() {  # <otf file> <px size> <symbol name> [glyph range]
  npx -y lv_font_conv@1.5.2 --no-compress --format lvgl --bpp 4 --size "$2" \
    --font "design/fonts/$1" -r "${4:-$range}" --lv-include lvgl.h -o "src/fonts/$3.c"
}
# <Grift weight> <px size> <symbol name> <fallback symbol name>: a text font plus its Noto fallback.
text_font() {
  local extra="grift_extra_$1" noto="noto_$1" noto_jp="noto_jp_$1"
  font "Grift-$1.otf" "$2" "$3" "$range,${!extra}"
  npx -y lv_font_conv@1.5.2 --no-compress --format lvgl --bpp 4 --size "$2" \
    --font "design/fonts/NotoSans-$1-subset.ttf" -r "${!noto}" \
    --font "design/fonts/NotoSansJP-$1-subset.ttf" -r "${!noto_jp}" \
    --lv-include lvgl.h -o "src/fonts/$4.c"
  # lv_font_conv 1.5 can't link a fallback itself.
  perl -0pi -e "s|(/\*Initialize a public general font descriptor\*/)|LV_FONT_DECLARE($4)\n\n\$1|; s|(\n\s*\.dsc = )|\n    .fallback = &$4,\$1|" "src/fonts/$3.c"
}
text_font SemiBold 14 grift_semibold_14 noto_semibold_14
text_font Medium 12 grift_medium_12 noto_medium_12
font Grift-SemiBold.otf 26 grift_semibold_26 "$clock_range"
font Grift-SemiBold.otf 21 grift_semibold_21 "$clock_range"
font Grift-Bold.otf 24 grift_bold_24 "$clock_range"

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

# MF DOOM clock tag, already cropped to the tag at its on-screen size (one color, tinted at runtime).
python3 -I tools/png_to_lvgl.py design/assets/doom_tag.png a8 doom_tag > src/assets/doom_tag.c
