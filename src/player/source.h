#pragma once

// Private to src/player/: what player_core.cpp needs from a music source. One source runs at a time,
// chosen by settings::source() at startup, and all of its methods run on the player task, which owns
// the network sessions.
#include <Arduino.h>

#include "player.h"

namespace player {

struct Poll {
  Status status;
  char title[sizeof(State::title)];
  char artist[sizeof(State::artist)];
  char album[sizeof(State::album)];
  uint32_t position_ms;
  uint32_t duration_ms;
  uint32_t polled_at_ms;  // millis() when position_ms was read
};

enum class Command : uint8_t { kTogglePlayPause, kNext, kPrevious, kWake };

class Source {
 public:
  enum class Result : uint8_t {
    kOk,          // out is filled in
    kFailed,      // a network or protocol error; three in a row show as "can't reach"
    kNotFound,    // there's nothing to talk to (no WiiM on the network)
    kNeedsSetup,  // the user has to do something on the settings page first (sign in to Spotify)
  };

  virtual ~Source() = default;
  // For messages: "WiiM", "Spotify".
  virtual const char* name() const = 0;
  virtual Result poll(Poll& out) = 0;
  // The current track's art after a successful poll; empty when it has none.
  virtual String artUrl() = 0;
  // `last` is the most recent successful poll.
  virtual void send(Command command, const Poll& last) = 0;
  // Closes network sessions: before an art download (two TLS sessions don't fit in RAM) and before
  // an update check.
  virtual void disconnect() = 0;
  // How long to wait before the next poll.
  virtual uint32_t pollIntervalMs(Status status) const = 0;
  // Called between polls: work handed over by other tasks (e.g. finishing a Spotify sign-in).
  virtual void service() {}
};

Source* wiimSource();
Source* spotifySource();

// Cuts the next poll's wait short (another task handed the source work to do).
void wake();

// Copies UTF-8 text without splitting a character when it doesn't fit.
void copyText(char* out, size_t size, const char* text);

}  // namespace player
