#include "wiim_text.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace wiim_text {
namespace {

constexpr char kEllipsis[] = "\xe2\x80\xa6";  // U+2026

int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Returns the byte count, or the input copied as-is if it isn't hex (some sources send plain text).
size_t fromHex(const char* hex, char* out, size_t out_size) {
  size_t len = strlen(hex);
  bool is_hex = len % 2 == 0;
  for (size_t i = 0; is_hex && i < len; i++) is_hex = hexDigit(hex[i]) >= 0;
  size_t n = 0;
  if (!is_hex) {
    for (; n + 1 < out_size && hex[n]; n++) out[n] = hex[n];
  } else {
    for (size_t i = 0; n + 1 < out_size && i < len; i += 2) out[n++] = hexDigit(hex[i]) << 4 | hexDigit(hex[i + 1]);
  }
  out[n] = '\0';
  return n;
}

// Drops a UTF-8 sequence or HTML entity left incomplete at the end by the WiiM's cut,
// then backs up to a word boundary unless that throws away more than half the text.
size_t cutToWord(char* s, size_t n) {
  size_t lead = n;
  while (lead > 0 && (s[lead - 1] & 0xC0) == 0x80) lead--;
  if (lead > 0 && (s[lead - 1] & 0x80)) {
    uint8_t b = s[lead - 1];
    size_t need = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : b >= 0xC0 ? 2 : 1;
    if (n - (lead - 1) < need) n = lead - 1;
  }
  for (size_t i = n; i > 0 && n - i < 10; i--) {
    if (s[i - 1] == ';') break;
    if (s[i - 1] == '&') {
      n = i - 1;
      break;
    }
  }

  size_t word = n;
  while (word > 0 && s[word] != ' ') word--;
  if (word > n / 2) n = word;
  while (n > 0 && strchr(" -,:;([&\"'", s[n - 1])) n--;
  s[n] = '\0';
  return n;
}

size_t putUtf8(char* out, uint32_t cp) {
  if (cp < 0x80) {
    out[0] = cp;
    return 1;
  }
  if (cp < 0x800) {
    out[0] = 0xC0 | cp >> 6;
    out[1] = 0x80 | (cp & 0x3F);
    return 2;
  }
  if (cp < 0x10000) {
    out[0] = 0xE0 | cp >> 12;
    out[1] = 0x80 | (cp >> 6 & 0x3F);
    out[2] = 0x80 | (cp & 0x3F);
    return 3;
  }
  out[0] = 0xF0 | cp >> 18;
  out[1] = 0x80 | (cp >> 12 & 0x3F);
  out[2] = 0x80 | (cp >> 6 & 0x3F);
  out[3] = 0x80 | (cp & 0x3F);
  return 4;
}

// Decodes entities in place; every entity is longer than the UTF-8 it decodes to.
void decodeEntities(char* s) {
  static const struct {
    const char* name;
    char c;
  } kNamed[] = {{"quot", '"'}, {"amp", '&'}, {"apos", '\''}, {"lt", '<'}, {"gt", '>'}};

  char* out = s;
  for (const char* in = s; *in;) {
    const char* semi = *in == '&' ? strchr(in, ';') : nullptr;
    if (semi && semi - in <= 9) {
      const char* name = in + 1;
      size_t name_len = semi - name;
      bool done = false;
      if (name_len > 1 && name[0] == '#') {
        bool hex = name[1] == 'x' || name[1] == 'X';
        char* end;
        uint32_t cp = strtoul(name + (hex ? 2 : 1), &end, hex ? 16 : 10);
        if (end == semi && cp > 0 && cp <= 0x10FFFF) {
          out += putUtf8(out, cp);
          done = true;
        }
      } else {
        for (const auto& e : kNamed) {
          if (strlen(e.name) == name_len && strncmp(name, e.name, name_len) == 0) {
            *out++ = e.c;
            done = true;
            break;
          }
        }
      }
      if (done) {
        in = semi + 1;
        continue;
      }
    }
    *out++ = *in++;
  }
  *out = '\0';
}

}  // namespace

void decode(const char* hex, char* out, size_t out_size) {
  char buf[kFieldLimit * 2 + 1];
  size_t n = fromHex(hex, buf, sizeof(buf));
  bool cut = n >= kFieldLimit;
  if (cut) cutToWord(buf, n);
  decodeEntities(buf);
  if (strncasecmp(buf, "unknow", 6) == 0 && (buf[6] == '\0' || strcasecmp(buf + 6, "n") == 0)) buf[0] = '\0';

  if (out_size == 0) return;
  size_t room = out_size - 1 - (cut ? sizeof(kEllipsis) - 1 : 0);
  size_t len = strlen(buf);
  if (len > room) {
    len = room;
    while (len > 0 && (buf[len] & 0xC0) == 0x80) len--;
  }
  memcpy(out, buf, len);
  out[len] = '\0';
  if (cut && len > 0) strcat(out, kEllipsis);
}

}  // namespace wiim_text
