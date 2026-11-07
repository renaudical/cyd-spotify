#include "album_art.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <JPEGDEC.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>

#include <memory>

namespace album_art {
namespace {

constexpr size_t kMaxDownload = 96 * 1024;  // LittleFS is 128 KB; Spotify's 300px art is ~30 KB
constexpr uint16_t kTimeoutMs = 5000;
// Downloads go to flash: a whole JPEG doesn't reliably fit in RAM next to a TLS session.
constexpr char kArtFile[] = "/art.jpg";

bool mountFilesystem() {
  static bool mounted = false;
  if (!mounted) mounted = LittleFS.begin(true);  // formats the partition on first boot
  if (!mounted) Serial.println("art: can't mount LittleFS");
  return mounted;
}

bool download(String url) {
  // Spotify's CDN serves each cover at 640px (ab67616d0000b273), 300px (ab67616d00001e02) and 64px.
  // 300px is a quarter of the download and still more than the 100px we show.
  url.replace("ab67616d0000b273", "ab67616d00001e02");

  WiFiClientSecure tls;
  tls.setInsecure();  // public images, only ever decoded into pixels
  tls.setHandshakeTimeout(kTimeoutMs / 1000);
  WiFiClient plain;
  HTTPClient http;
  http.setConnectTimeout(kTimeoutMs);
  http.setTimeout(kTimeoutMs);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  bool ok = url.startsWith("https://") ? http.begin(tls, url) : http.begin(plain, url);
  if (!ok) return false;

  int code = http.GET();
  int size = http.getSize();
  if (code != HTTP_CODE_OK || size > (int)kMaxDownload) {
    Serial.printf("art: download failed (%d, %d bytes)\n", code, size);
    http.end();
    return false;
  }
  File file = LittleFS.open(kArtFile, FILE_WRITE);
  if (!file) {
    http.end();
    return false;
  }
  int written = http.writeToStream(&file);
  file.close();
  http.end();
  if (written <= 0) Serial.printf("art: download failed (%s)\n", http.errorToString(written).c_str());
  return written > 0;
}

// JPEGDEC's own File support isn't compiled in for this core, so it reads through these.
void closeFile(void* handle) { static_cast<File*>(handle)->close(); }
int32_t readFile(JPEGFILE* f, uint8_t* buf, int32_t len) {
  int32_t n = static_cast<File*>(f->fHandle)->read(buf, len);
  f->iPos += n;
  return n;
}
int32_t seekFile(JPEGFILE* f, int32_t pos) {
  if (!static_cast<File*>(f->fHandle)->seek(pos)) return -1;
  f->iPos = pos;
  return pos;
}

// Scales the decoded image to the output size while it decodes, so the full-size picture is never in
// RAM: the decoder fills one band of rows at a time, each finished row is averaged into the output row
// it belongs to, and each finished output row goes to the sink. The image is cropped to a centered
// square first. Decoding runs only on the network task, so this lives in a file-static.
struct Scaler {
  int size;  // output side
  RowSink sink;
  void* context;
  int w, h;          // decoded image size
  int side, x0, y0;  // centered square crop
  uint16_t* band;    // kMaxBandRows full-width rows from the decoder
  int band_y, band_rows;
  int next_row;    // next output row to finish
  uint16_t* sums;  // r, g, b, count per pixel of the output row being built
  uint16_t* row;   // the finished output row
};
constexpr int kMaxBandRows = 16;  // the tallest JPEG MCU
Scaler sc;

int spanStart(int i, int origin) { return origin + i * sc.side / sc.size; }
int spanEnd(int i, int origin) { return max(spanStart(i, origin) + 1, origin + (i + 1) * sc.side / sc.size); }

void addRow(const uint16_t* row, int y) {
  while (sc.next_row < sc.size) {
    int y0 = spanStart(sc.next_row, sc.y0), y1 = spanEnd(sc.next_row, sc.y0);
    if (y < y0) return;  // cropped off, or before this output row
    for (int x = 0; x < sc.size; x++) {
      uint16_t* s = sc.sums + x * 4;
      for (int sx = spanStart(x, sc.x0); sx < spanEnd(x, sc.x0); sx++) {
        uint16_t p = row[sx];
        s[0] += p >> 11;
        s[1] += p >> 5 & 0x3F;
        s[2] += p & 0x1F;
        s[3]++;
      }
    }
    if (y + 1 < y1) return;  // this output row needs more input rows

    for (int x = 0; x < sc.size; x++) {
      uint16_t* s = sc.sums + x * 4;
      sc.row[x] = (s[0] / s[3]) << 11 | (s[1] / s[3]) << 5 | (s[2] / s[3]);
    }
    sc.sink(sc.next_row, sc.row, sc.context);
    memset(sc.sums, 0, sc.size * 4 * sizeof(uint16_t));
    sc.next_row++;  // when enlarging, the same input row also feeds the next output row
  }
}

void flushBand() {
  for (int i = 0; i < sc.band_rows; i++) addRow(sc.band + i * sc.w, sc.band_y + i);
  sc.band_rows = 0;
}

int draw(JPEGDRAW* d) {
  if (d->y != sc.band_y) {
    flushBand();
    sc.band_y = d->y;
  }
  int rows = min(min(d->iHeight, kMaxBandRows), sc.h - d->y);
  int n = min(d->iWidth, sc.w - d->x);
  for (int y = 0; y < rows && n > 0; y++) {
    memcpy(sc.band + y * sc.w + d->x, d->pPixels + y * d->iWidth, n * sizeof(uint16_t));
  }
  sc.band_rows = max(sc.band_rows, rows);
  return 1;
}

void copyRow(int y, const uint16_t* pixels, void* out) {
  memcpy(static_cast<uint16_t*>(out) + y * kSize, pixels, kSize * sizeof(uint16_t));
}

}  // namespace

bool fetch(const char* url, uint16_t* out) { return mountFilesystem() && download(url) && decode(kSize, copyRow, out); }

bool decode(int size, RowSink row, void* context) {
  std::unique_ptr<JPEGDEC> jpeg(new (std::nothrow) JPEGDEC);  // ~17 KB, too big for the task stack
  if (!jpeg) {
    Serial.println("art: no memory for the JPEG decoder");
    return false;
  }
  File file = LittleFS.open(kArtFile, FILE_READ);
  if (!file) return false;
  if (!jpeg->open(&file, file.size(), closeFile, readFile, seekFile, draw)) {
    Serial.printf("art: can't open JPEG (error %d, %u bytes)\n", jpeg->getLastError(), (unsigned)file.size());
    return false;
  }
  jpeg->setPixelType(RGB565_LITTLE_ENDIAN);

  // Decode at the smallest of 1/1, 1/2, 1/4, 1/8 that still covers the output. Progressive JPEGs only
  // decode at 1/8 in JPEGDEC.
  static const int kScale[] = {0, JPEG_SCALE_HALF, JPEG_SCALE_QUARTER, JPEG_SCALE_EIGHTH};
  int w = jpeg->getWidth(), h = jpeg->getHeight();
  int shift = 0;
  while (shift < 3 && min(w, h) >> (shift + 1) >= size) shift++;
  if (jpeg->getJPEGType() == JPEG_MODE_PROGRESSIVE) shift = 3;

  sc = {};
  sc.size = size;
  sc.sink = row;
  sc.context = context;
  sc.w = (w + (1 << shift) - 1) >> shift;
  sc.h = (h + (1 << shift) - 1) >> shift;
  sc.side = min(sc.w, sc.h);
  sc.x0 = (sc.w - sc.side) / 2;
  sc.y0 = (sc.h - sc.side) / 2;
  // One allocation for the band, the row sums and the output row: ~12 KB for full-screen art.
  sc.band = static_cast<uint16_t*>(malloc((sc.w * kMaxBandRows + size * 5) * sizeof(uint16_t)));
  if (!sc.band) {
    Serial.printf("art: no memory to decode %dx%d\n", sc.w, sc.h);
    jpeg->close();
    return false;
  }
  sc.sums = sc.band + sc.w * kMaxBandRows;
  sc.row = sc.sums + size * 4;
  memset(sc.sums, 0, size * 4 * sizeof(uint16_t));

  bool ok = jpeg->decode(0, 0, kScale[shift]);
  flushBand();
  jpeg->close();
  ok = ok && sc.next_row == size;
  if (!ok) Serial.printf("art: decode failed (error %d)\n", jpeg->getLastError());
  free(sc.band);
  sc.band = nullptr;
  return ok;
}

}  // namespace album_art
