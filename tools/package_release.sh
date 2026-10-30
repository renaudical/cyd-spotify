#!/usr/bin/env bash
# Packages a build (pio run) into dist/ for a release:
#   cyd-spotify-ota.bin   the app image devices download and install over the air
#   cyd-spotify-full.bin  everything merged at offset 0, for flashing by hand (erases settings)
#   site/                 the web installer's firmware parts and manifest (written part by part, so a
#                         reinstall keeps the settings in nvs unless the user chooses to erase)
# Usage: tools/package_release.sh <version, e.g. 1.2.0>
set -euo pipefail
cd "$(dirname "$0")/.."

version=${1:?usage: tools/package_release.sh <version>}
build=.pio/build/cyd
boot_app0="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

rm -rf dist
mkdir -p dist/site/firmware
cp "$build/firmware.bin" dist/cyd-spotify-ota.bin

# Offsets match the bootloader, partitions.csv and what `pio run -t upload` writes.
parts=(0x1000 "$build/bootloader.bin" 0x8000 "$build/partitions.bin" 0xe000 "$boot_app0" 0x10000 "$build/firmware.bin")
pio pkg exec -p tool-esptoolpy -- esptool.py --chip esp32 merge_bin -o dist/cyd-spotify-full.bin \
  --flash_mode dio --flash_freq 40m --flash_size 4MB "${parts[@]}" > /dev/null

cp "$build/bootloader.bin" "$build/partitions.bin" "$boot_app0" "$build/firmware.bin" dist/site/firmware/
cat > dist/site/manifest.json <<JSON
{
  "name": "Now Playing Brickman",
  "version": "$version",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32",
      "parts": [
        { "path": "firmware/bootloader.bin", "offset": 4096 },
        { "path": "firmware/partitions.bin", "offset": 32768 },
        { "path": "firmware/boot_app0.bin", "offset": 57344 },
        { "path": "firmware/firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
JSON
echo "Packaged $version in dist/"
