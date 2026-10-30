#pragma once

#include <stdint.h>

namespace album_art {

// Album art is always delivered at exactly this size, to fit the Now Playing art box.
constexpr int kSize = 100;

// Downloads the JPEG at url and scales it to fill a kSize x kSize RGB565 image (little-endian,
// LVGL's layout), cropping the longer side. Blocking; call from the network task.
bool fetch(const char* url, uint16_t* out);

}  // namespace album_art
