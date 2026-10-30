// Copy to include/secrets.h (gitignored) and fill in.
#pragma once

#define WIFI_SSID "your-network"
#define WIFI_PASSWORD "your-password"
// The WiiM's IP address on your network (WiiM Home app > device settings > Network). Give it a
// DHCP reservation in your router so it doesn't change.
#define WIIM_HOST "192.168.1.50"
// Optional: POSIX TZ string for the screensaver clock, e.g. US Central below. Leave it out and the
// device detects its timezone from its public IP address (wrong behind a VPN).
// #define TIMEZONE "CST6CDT,M3.2.0,M11.1.0"
