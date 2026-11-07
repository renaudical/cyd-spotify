#pragma once

#include <stdint.h>

namespace album_art {

// Album art is always delivered at exactly this size, to fit the Now Playing art box.
constexpr int kSize = 100;
// The album art screensaver's square, the full height of the screen.
constexpr int kFullScreenSize = 240;

// Downloads the JPEG at url and scales it to fill a kSize x kSize RGB565 image (little-endian,
// LVGL's layout), cropping the longer side. Blocking; call from the network task.
bool fetch(const char* url, uint16_t* out);

// Receives the scaled image one row at a time, top to bottom: `size` RGB565 pixels (little-endian).
using RowSink = void (*)(int y, const uint16_t* pixels, void* context);

// Decodes the art from the last successful fetch() again, scaled to fill a size x size square (up to
// kFullScreenSize), without ever holding the whole image in RAM. Same task as fetch().
bool decode(int size, RowSink row, void* context);

}  // namespace album_art
