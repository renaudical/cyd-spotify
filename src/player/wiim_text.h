#pragma once

#include <stddef.h>

// Text fields from the WiiM's getPlayerStatus. No Arduino dependencies, so it can be tested on a computer.
namespace wiim_text {

// The WiiM cuts every text field to this many bytes, even mid-word, mid-character or mid-entity.
constexpr size_t kFieldLimit = 64;

// Decodes a field (hex-encoded UTF-8 containing HTML entities like &quot;) into out. A field that hit
// kFieldLimit is cut back to a whole word and ends in "…". "Unknown" placeholders become "".
void decode(const char* hex, char* out, size_t out_size);

}  // namespace wiim_text
