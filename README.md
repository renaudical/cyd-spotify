# Now Playing Brickman

A little "now playing" display and remote for Spotify, built on the ESP32 "Cheap Yellow Display". It shows the song, artist, album art and progress, with play/pause, next and previous buttons. When nobody touches it for a while, it switches to a themed clock. A new theme arrives every month, delivered over Wi-Fi.

## What you need

- An **ESP32-2432S028R** ("Cheap Yellow Display", CYD): 2.8" 320×240 touch screen with an ESP32. Get the micro-USB version: some USB-C versions use a different display chip and need changes.
- A USB **data** cable (some cables only charge).
- A computer with **Chrome or Edge** (the installer uses Web Serial; Safari and Firefox don't support it).
- **Spotify Premium**, or a **WiiM** streamer (see "Where the music comes from").

## Install

1. Plug the display into your computer.
2. Open **https://renaudical.github.io/cyd-spotify/**, click **Install** and pick the serial port (often "USB Serial" or "CH340").
3. When it asks, enter your Wi-Fi name and password (2.4 GHz networks only; the ESP32 has no 5 GHz radio).

If no port shows up, install the [CH340 driver](https://www.wch-ic.com/downloads/CH341SER_EXE.html) (Windows and older macOS) and try again.

You can also skip Wi-Fi in the installer. The display then shows a QR code: scan it with your phone to join the `Brickman-XXXX` network, and a setup page opens.

## Settings

Everything else is set on the display's own web page. Hold the album art for 2 seconds: the theme picker opens, and its bottom line shows the display's address (for example `192.168.1.23`). Open that address in a browser on the same network, or use `http://brickman-xxxx.local`.

The page sets Wi-Fi, the music source, the clock's time zone (detected automatically, but you can pick one), turns the screen upside down if your build has the USB ports on the other side, and restarts the display.

## Where the music comes from

### Spotify (Premium)

The display reads your Spotify account directly. Spotify requires everyone to use their own (free) developer app, which takes about two minutes:

1. Open [developer.spotify.com/dashboard](https://developer.spotify.com/dashboard), log in and click **Create app**.
2. Name it anything. As **Redirect URI**, add exactly:
   `https://renaudical.github.io/cyd-spotify/spotify-callback.html`
3. Tick **Web API**, agree to the terms and save.
4. Copy the app's **Client ID** into the display's settings page and click **Connect Spotify**.

Spotify asks you to log in and approve. You then land back on the settings page. Your password never reaches the display; it only keeps a sign-in token.

By default it shows whatever your account is playing, on any device. To show only one speaker (say, the living room), pick it under **Show music playing on**. It only appears in the list while Spotify can see it (switched on and online).

Spotify allows up to 5 accounts per developer app, so a household can share one app: add other people's Spotify emails under **User Management** in the app's settings.

### WiiM (or another LinkPlay streamer)

Choose WiiM on the settings page. The display finds the streamer on your network by itself, or you can enter its address. It shows whatever that streamer plays, from any service, without Spotify's limits.

## Themes and updates

Hold the album art for 2 seconds to pick a theme. When a new version comes out, the display installs it on its own while it's showing the clock, and restarts. New themes appear in the picker; your choice never changes by itself.

## Building it yourself

This is a [PlatformIO](https://platformio.org) project (Arduino framework, LVGL 9). With the display plugged in:

```sh
pio run -t upload
```

Copy `include/secrets.example.h` to `include/secrets.h` to give a development board its Wi-Fi and settings without the web page. `CLAUDE.md` describes the code. [docs/adding-a-theme.md](docs/adding-a-theme.md) covers how to make a theme.

## Credits

UI font: Grift, used with the permission of its designer. Display drivers: [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) and [LVGL](https://lvgl.io). Album art decoding: [JPEGDEC](https://github.com/bitbank2/JPEGDEC).
