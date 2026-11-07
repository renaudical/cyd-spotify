#include "palette.h"

#include <algorithm>

namespace palette {
namespace {

constexpr int kStep = 4;        // sample every 4th pixel each way: 625 of a 100x100 image
constexpr int kIterations = 6;  // k-means settles well within this on so few samples
constexpr int kMinPeak = 140;   // the brightest channel of each color, so none vanish on black

struct Color {
  int r, g, b;
};

Color sample(uint16_t p) { return {(p >> 11) * 255 / 31, (p >> 5 & 0x3F) * 255 / 63, (p & 0x1F) * 255 / 31}; }

// Weighted toward how the eye tells colors apart (green most, blue least).
int distance(const Color& a, const Color& b) {
  int dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
  return 2 * dr * dr + 4 * dg * dg + 3 * db * db;
}

}  // namespace

void fromArt(const uint16_t* pixels, int width, int height, Rgb out[kColors]) {
  // Seeds: the average color, then each next one the sample farthest from the seeds so far, so a
  // small splash of a strong color still gets one.
  Color centers[kColors];
  long r = 0, g = 0, b = 0, n = 0;
  for (int y = 0; y < height; y += kStep) {
    for (int x = 0; x < width; x += kStep) {
      Color c = sample(pixels[y * width + x]);
      r += c.r, g += c.g, b += c.b, n++;
    }
  }
  if (n == 0) n = 1;
  centers[0] = {int(r / n), int(g / n), int(b / n)};
  for (int k = 1; k < kColors; k++) {
    int best = -1;
    for (int y = 0; y < height; y += kStep) {
      for (int x = 0; x < width; x += kStep) {
        Color c = sample(pixels[y * width + x]);
        int nearest = distance(c, centers[0]);
        for (int j = 1; j < k; j++) nearest = std::min(nearest, distance(c, centers[j]));
        if (nearest > best) best = nearest, centers[k] = c;
      }
    }
  }

  long counts[kColors] = {};
  for (int it = 0; it < kIterations; it++) {
    long sums[kColors][3] = {};
    for (long& c : counts) c = 0;
    for (int y = 0; y < height; y += kStep) {
      for (int x = 0; x < width; x += kStep) {
        Color c = sample(pixels[y * width + x]);
        int nearest = 0;
        for (int k = 1; k < kColors; k++) {
          if (distance(c, centers[k]) < distance(c, centers[nearest])) nearest = k;
        }
        sums[nearest][0] += c.r, sums[nearest][1] += c.g, sums[nearest][2] += c.b;
        counts[nearest]++;
      }
    }
    for (int k = 0; k < kColors; k++) {
      if (counts[k]) centers[k] = {int(sums[k][0] / counts[k]), int(sums[k][1] / counts[k]), int(sums[k][2] / counts[k])};
    }
  }

  // Most common first (insertion sort; there are four).
  for (int i = 1; i < kColors; i++) {
    for (int j = i; j > 0 && counts[j] > counts[j - 1]; j--) {
      long t = counts[j];
      counts[j] = counts[j - 1], counts[j - 1] = t;
      Color c = centers[j];
      centers[j] = centers[j - 1], centers[j - 1] = c;
    }
  }

  for (int k = 0; k < kColors; k++) {
    Color c = centers[k];
    int peak = std::max({c.r, c.g, c.b});
    if (peak < kMinPeak) {
      if (peak < 8) c = {kMinPeak, kMinPeak, kMinPeak};  // black art: a soft gray
      else c = {c.r * kMinPeak / peak, c.g * kMinPeak / peak, c.b * kMinPeak / peak};
    }
    out[k] = {uint8_t(c.r), uint8_t(c.g), uint8_t(c.b)};
  }
}

}  // namespace palette
