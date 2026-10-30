#pragma once

#include <stdint.h>

// Over-the-air updates from GitHub Pages. A task on core 0 fetches ota.json (published by the release
// workflow) while the screensaver is up, and if it names a newer version, downloads it into the idle app
// slot and restarts. The new version confirms itself once it has fetched ota.json too; if it restarts
// before that, the bootloader rolls back to the previous one.
namespace updater {

void begin();

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
