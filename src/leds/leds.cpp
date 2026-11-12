#include "leds.h"

#include <FastLED.h>
#include <esp_heap_caps.h>

namespace leds {
namespace {

constexpr int kPin = 27;    // CN1's IO27, through a 330 ohm resistor
constexpr int kCount = 50;  // the strand as cut; a shorter one just leaves the extra data unused
constexpr uint32_t kMaxMilliamps = 450;  // the LEDs' share of the USB supply; FastLED dims to stay under
constexpr uint8_t kMusicBrightness = 140;
constexpr uint8_t kWhiteFloor = 6, kWhiteMin = 30, kWhiteMax = 110;  // twinkling: dimmer, each LED its own peak
constexpr uint32_t kFrameMs = 20;      // 50 fps
constexpr uint32_t kNewAlbumMs = 2000;  // a new album's colors crossfade in over this
constexpr uint8_t kFadeStep = 3;       // per frame: switching modes or on/off takes about 1.7 s

// How fast things move, in noise cells per 100 s (about one swing each way per cell); smaller is calmer.
constexpr uint32_t kColorDrift = 9;  // around the loop of album colors
constexpr uint32_t kBreath = 34;     // each LED's brightness while music plays
constexpr uint32_t kTwinkle = 68;    // each white LED's sparkle

constexpr palette::Rgb kNoArt[palette::kColors] = {{40, 60, 255}, {120, 40, 255}, {60, 120, 255}, {160, 60, 220}};

CRGB pixels[kCount];
uint8_t white_peak[kCount];
TaskHandle_t task = nullptr;

// Set by the UI task, read by the LED task.
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
bool enabled = false;
bool playing = false;
CRGB target[palette::kColors];
uint32_t target_serial = 0;  // bumped with each new set of colors

// A little more saturated, and full brightness, so the figure glows in color rather than pastel.
// Near-grays stay as they are: boosting them would invent a hue.
CRGB vivid(const palette::Rgb& c) {
  CHSV hsv = rgb2hsv_approximate(CRGB(c.r, c.g, c.b));
  if (hsv.s > 40) hsv.s = qadd8(hsv.s, 60);
  hsv.v = 255;
  return hsv;
}

// Smooth noise in 0-255. inoise16 rather than inoise8, whose coarser steps show as jitter on LEDs;
// stretched, since it rarely reaches its extremes. `rate` is in cells per 100 s.
uint8_t noise(uint32_t seed, uint32_t t_ms, uint32_t rate) {
  // 64-bit, then cut to 32: the noise repeats every 256 cells, so the cut doesn't show.
  uint32_t y = uint64_t(t_ms) * rate * 65536 / 100000;
  int32_t n = inoise16(seed << 16, y, 0);
  return constrain((n - 14000) * 255 / 37000, 0, 255);
}

// Somewhere on the loop the four colors make, each one blending into the next. `pos` is 16-bit so the
// blends have 256 steps.
CRGB onLoop(const CRGB colors[4], uint16_t pos) {
  uint8_t i = pos >> 14;
  return blend(colors[i], colors[(i + 1) % 4], uint8_t(pos >> 6));
}

void run(void*) {
  FastLED.addLeds<WS2812B, kPin, BGR>(pixels, kCount);  // this strand takes blue, green, red
  FastLED.setMaxPowerInVoltsAndMilliamps(5, kMaxMilliamps);
  FastLED.setDither(DISABLE_DITHER);  // dithering flickers at the twinkle's low levels
  for (uint8_t& peak : white_peak) peak = random8(kWhiteMin, kWhiteMax);
  Serial.printf("leds: %d on GPIO %d (free heap %u)\n", kCount, kPin, (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));

  CRGB from[palette::kColors], to[palette::kColors], shown[palette::kColors];
  for (int k = 0; k < palette::kColors; k++) from[k] = to[k] = shown[k] = vivid(kNoArt[k]);
  uint32_t serial = 0, crossfade_start = 0;
  uint8_t music = 0;  // 0 = white twinkling, 255 = album colors
  uint8_t on = 0;     // fades in and out
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    uint32_t t = millis();
    portENTER_CRITICAL(&lock);
    bool want_on = enabled, want_music = playing;
    if (target_serial != serial) {  // new colors: crossfade from whatever is showing now
      serial = target_serial;
      for (int k = 0; k < palette::kColors; k++) from[k] = shown[k], to[k] = target[k];
      crossfade_start = t;
    }
    portEXIT_CRITICAL(&lock);

    uint32_t elapsed = t - crossfade_start;
    uint8_t progress = elapsed >= kNewAlbumMs ? 255 : ease8InOutCubic(elapsed * 255 / kNewAlbumMs);
    for (int k = 0; k < palette::kColors; k++) shown[k] = blend(from[k], to[k], progress);
    music = want_music ? qadd8(music, kFadeStep) : qsub8(music, kFadeStep);
    on = want_on ? qadd8(on, kFadeStep) : qsub8(on, kFadeStep);

    for (int i = 0; i < kCount; i++) {
      uint16_t pos = noise(i * 3, t, kColorDrift) * 256;  // once round the loop of colors
      uint8_t level = scale8(map(noise(i * 3 + 1, t, kBreath), 0, 255, 50, 255), kMusicBrightness);
      CRGB album = onLoop(shown, pos).nscale8_video(level);
      // Squaring the noise keeps each LED mostly low, with brief brighter glints.
      uint8_t white = kWhiteFloor + scale8(white_peak[i] - kWhiteFloor, dim8_raw(noise(i * 3 + 2, t, kTwinkle)));
      pixels[i] = blend(CRGB(white, white, white), album, ease8InOutCubic(music));
    }
    FastLED.setBrightness(ease8InOutCubic(on));
    FastLED.show();

    if (!want_on && on == 0) {  // dark: sleep until switched on again
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      wake = xTaskGetTickCount();
    }
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(kFrameMs));
  }
}

}  // namespace

void setEnabled(bool on) {
  portENTER_CRITICAL(&lock);
  enabled = on;
  portEXIT_CRITICAL(&lock);
  if (!on) return;
  // Core 1, away from Wi-Fi on core 0, so its interrupts can't disturb the LED timing; above the UI's
  // priority, so a long screen redraw can't hold up a frame.
  if (!task) xTaskCreatePinnedToCore(run, "leds", 3072, nullptr, 2, &task, 1);
  else xTaskNotifyGive(task);
}

void setPlaying(bool music) {
  portENTER_CRITICAL(&lock);
  playing = music;
  portEXIT_CRITICAL(&lock);
}

void setColors(const palette::Rgb* colors) {
  CRGB next[palette::kColors];
  for (int k = 0; k < palette::kColors; k++) next[k] = vivid(colors ? colors[k] : kNoArt[k]);
  portENTER_CRITICAL(&lock);
  bool same = true;
  for (int k = 0; k < palette::kColors; k++) same = same && next[k] == target[k];
  if (!same) {  // the UI resends colors on any state change; only a real change restarts the crossfade
    for (int k = 0; k < palette::kColors; k++) target[k] = next[k];
    target_serial++;
  }
  portEXIT_CRITICAL(&lock);
}

}  // namespace leds
