#pragma once

#include <stddef.h>

// The screensaver clock's local time. NTP only gives UTC, so the timezone comes from TIMEZONE in
// secrets.h if set, otherwise from an IP geolocation lookup (saved in NVS, so later boots don't wait on it).
namespace tz {

struct Zone {
  const char* name;   // IANA, e.g. "America/Chicago"
  const char* posix;  // e.g. "CST6CDT,M3.2.0,M11.1.0"
};

// Generated table (tz_table.cpp, from tools/gen_timezones.py).
extern const Zone kZones[];
extern const size_t kZoneCount;

// POSIX rule for an IANA name, or nullptr if unknown.
const char* lookup(const char* iana);

// True once local time can be trusted: TIMEZONE is set, or a zone was saved or detected.
// Until then the clock is hidden rather than showing UTC. Safe to call from any task.
bool known();

// Called by the net task whenever Wi-Fi is up: starts SNTP once, then detects the zone if needed,
// retrying with backoff. Blocks for at most a few seconds per attempt.
void poll();

}  // namespace tz
