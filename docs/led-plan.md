# Glow Brickman: add the LED strand to the CYD music player

Paste this whole file into Claude Code (or save it in the firmware project as `LED_PLAN.md` and ask Claude Code to follow it).

## What this project is

The CYD (ESP32-2432S028R "Cheap Yellow Display", 2.8") runs my "Now Playing" music player: album art, artist, title and album, with back, play/pause and skip controls, and a screensaver after about 10 s idle. It sits in the chest of a 36 cm 3D-printed Brickman figure printed in clear PETG.

I've added an addressable LED strand that runs through the inside of the figure (torso, arms, hips, legs and head). The LEDs should glow in colours taken from the current album art, with a slow, subtle "breathing" movement so the figure feels alive.

## Hardware: confirmed working, don't change

- LED strand: Adafruit NeoPixel Pebble/Seed strand #6026 (WS2812-compatible, 5 V, 10 cm between LEDs).
- **Data: GPIO 27** (CN1 connector), through a 330 Ω resistor. Confirmed working with a FastLED test sketch.
- Power: 5 V from the P1 connector's VIN pin (measured 4.88 V), with a 1000 µF capacitor across 5 V and GND.
- Everything runs off the board's USB power, so **LED current must be capped** (see below).
- Pins to leave alone: GPIO 21 (screen backlight), and GPIO 4/16/17 (the board's own RGB LED; fine to use, but don't reuse them for anything else).

## Step 1: fix the colour order (do this first)

The test sketch used `FastLED.addLeds<WS2812B, 27, GRB>`. The strand lights, but its colours don't match what was asked for. Re-flash the test sketch below with `RGB`. If the strand still doesn't match the board's own light, ask me which colour the strand shows for each board colour, and pick the matching FastLED order (RGB, RBG, GRB, GBR, BRG or BGR).

```cpp
#include <FastLED.h>
#define LED_PIN  27
#define NUM_LEDS 10
CRGB leds[NUM_LEDS];
#define R_PIN 4
#define G_PIN 16
#define B_PIN 17
void board(bool r, bool g, bool b) { digitalWrite(R_PIN, !r); digitalWrite(G_PIN, !g); digitalWrite(B_PIN, !b); }
void setup() {
  pinMode(R_PIN, OUTPUT); pinMode(G_PIN, OUTPUT); pinMode(B_PIN, OUTPUT);
  FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);   // <- colour order under test
  FastLED.setBrightness(40);
}
void loop() {
  board(1,0,0); fill_solid(leds, NUM_LEDS, CRGB::Red);   FastLED.show(); delay(700);
  board(0,1,0); fill_solid(leds, NUM_LEDS, CRGB::Green); FastLED.show(); delay(700);
  board(0,0,1); fill_solid(leds, NUM_LEDS, CRGB::Blue);  FastLED.show(); delay(700);
}
```

Write down the colour order that matches. It's used in every step after this.

## Step 2: add the LEDs to the real music-player firmware

Add an LED module to the existing player firmware. Don't rewrite the player.

**Setup**
- FastLED on GPIO 27, with the colour order found in Step 1.
- `NUM_LEDS`: make this one easy-to-change constant. I'll count the LEDs once the strand is threaded through the figure (expect about 30–45). Start at 40.
- Power cap: `FastLED.setMaxPowerInVoltsAndMilliamps(5, 450);` and a base brightness of about 140. This keeps the board plus LEDs safely under the USB supply.

**The animation**
- Each LED's colour comes from a palette of 3–4 colours taken from the current album art.
- A slow noise function makes each LED's brightness and colour drift over a few seconds, so soft waves move through the body. LEDs never go fully dark.
- When the track changes, the palette crossfades to the new album over about 2 seconds.
- **Non-blocking:** update at about 50 fps using `EVERY_N_MILLISECONDS(20)` or millis(). Never use `delay()`. The screen, touch and network code must keep running smoothly.

Reference animation code:

```cpp
CRGBPalette16 currentPal, targetPal;

void setAlbumColors(CRGB a, CRGB b, CRGB c, CRGB d) {
  targetPal = CRGBPalette16(a,b,c,d, a,b,c,d, a,b,c,d, a,b,c,d);
}

void updateLeds() {                       // call every loop()
  EVERY_N_MILLISECONDS(20) {
    nblendPaletteTowardPalette(currentPal, targetPal, 12);   // ~2 s crossfade
    uint32_t t = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      uint8_t colour = inoise8(i * 40, t / 40);              // which album colour, drifting slowly
      uint8_t level  = inoise8(i * 60 + 5000, t / 12);       // the "breathing"
      level = map(level, 0, 255, 40, 255);                   // never fully dark
      leds[i] = ColorFromPalette(currentPal, colour, level, LINEARBLEND);
    }
    FastLED.show();
  }
}
```

- Tuning: bigger divisors (`t / 12`, `t / 40`) mean slower, calmer movement. Expose them as constants.

**Getting the colours from the album art**
- Use the album-art image the player already downloads and decodes. Downsample it (for example to 16×16) and pick 3–4 dominant colours with simple binning or k-means.
- Skip near-black, near-grey and near-white colours so the glow stays colourful. Boost saturation a little.
- If the art fails to load, or nothing is playing, fall back to a calm default palette (for example soft blue and purple).
- Call `setAlbumColors()` whenever the art changes.

**Behaviour with the screensaver and when paused**
- Paused or idle: keep the animation running, but dim to about 40% and slow it down.
- Playing: normal brightness.

## Step 3: check

- The player UI, touch and screensaver work exactly as before.
- The LEDs breathe smoothly with no stutter, and the screen doesn't stutter.
- Changing tracks crossfades the colours to the new album.
- Nothing gets warm, and the board doesn't reset when the LEDs are bright. If it resets, lower the 450 mA cap.

## Status (2026-11-12)

- Step 1 is done: this strand's colour order is **BGR**.
- Step 2 is built as the **LED Lights** theme (`src/leds/leds.cpp`): the LEDs only run with that theme, on their own task rather than `loop()`, so the full-screen album art draw (~220 ms with the display locked) can't freeze them. When music isn't playing they twinkle softly in white instead of dimming the album colors.
- The strand was cut to 50 LEDs and is threaded as one piece, looped in and out of each part (head, torso, arms, hips, left leg, right leg last), so there are no group joints to solder. The LED map comes from a walk test after threading.
