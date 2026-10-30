#pragma once

#include <stdint.h>

// Over-the-air updates from GitHub releases. While the screensaver is up, the newest release is checked
// every few hours; if it's newer, its firmware is downloaded into the idle app slot, checked against the
// release's SHA-256 digest, and the device restarts. The new version confirms itself once it has reached
// GitHub too; if it restarts before that, the bootloader rolls back to the previous one.
namespace updater {

void begin();

// Checks run on the player task, between polls and with its connection closed: a TLS session needs
// most of the free RAM, and the player task has a stack to spare. check() can take seconds, or install
// and restart.
bool due();
void check();

// The UI reports whether the screensaver is showing; checks and installs only happen then.
void setIdle(bool idle);

// True from the start of a download until the restart (or failure); the UI shows progress meanwhile.
bool installing();
uint8_t percent();
// The version being installed, e.g. "1.2.0".
const char* targetVersion();

// This build's version, e.g. "1.2.0", or "dev" for local builds (which never install updates).
const char* currentVersion();

}  // namespace updater
