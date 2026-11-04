// Optional, for local development: copy to include/secrets.h (gitignored). On first boot these seed any
// settings that are still empty, so a freshly flashed dev board joins Wi-Fi without the setup page.
// Release builds have no secrets.h; people set everything up on the device's web page or, for Wi-Fi,
// in the web installer. Settings live in NVS from then on and are edited at http://brickman-xxxx.local/.
#pragma once

#define WIFI_SSID "your-network"
#define WIFI_PASSWORD "your-password"
// Optional: which WiiM, by its network name (as the settings page lists it) or address. Leave it out
// and the device uses the first WiiM it finds on the network. A name keeps working when the router
// gives the WiiM a new address.
// #define WIIM_HOST "WiiM-Mini-1A2B"
// Optional: POSIX TZ string for the screensaver clock, e.g. US Central below. Leave it out and the
// device detects its timezone from its public IP address (wrong behind a VPN).
// #define TIMEZONE "CST6CDT,M3.2.0,M11.1.0"
