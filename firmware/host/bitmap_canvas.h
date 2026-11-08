// Host-only Canvas that draws into a plain 1-bit bitmap.
//
// Used by the tests and by render_screens to check what the screens would
// draw without any hardware. It also counts every draw that falls outside the
// bitmap and every present() call, which the tests assert on.
//
// Honest limits: triangle and circle edges are rasterised by this file, not by
// Adafruit GFX, so edge pixels can differ by about one pixel from the real
// display. Layout, proportions and recognisability are what this verifies.
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "font5x7.h"
#include "ui/canvas.h"

namespace hosttest {

class BitmapCanvas : public motonav::Canvas {
 public:
  BitmapCanvas(int w, int h) : w_(w), h_(h), px_(static_cast<size_t>(w) * h, 0) {}

  int width() const override { return w_; }
  int height() const override { return h_; }

  void clear() override { std::fill(px_.begin(), px_.end(), 0); }

  void fillRect(int x, int y, int w, int h, bool on) override {
    for (int yy = y; yy < y + h; ++yy)
      for (int xx = x; xx < x + w; ++xx) set(xx, yy, on);
  }

  // Inclusive of edges, like a scanline fill.
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool on) override {
    const int minX = std::min(x0, std::min(x1, x2)), maxX = std::max(x0, std::max(x1, x2));
    const int minY = std::min(y0, std::min(y1, y2)), maxY = std::max(y0, std::max(y1, y2));
    for (int y = minY; y <= maxY; ++y) {
      for (int x = minX; x <= maxX; ++x) {
        const long e0 = edge(x0, y0, x1, y1, x, y);
        const long e1 = edge(x1, y1, x2, y2, x, y);
        const long e2 = edge(x2, y2, x0, y0, x, y);
        const bool neg = e0 < 0 || e1 < 0 || e2 < 0;
        const bool pos = e0 > 0 || e1 > 0 || e2 > 0;
        if (!(neg && pos)) set(x, y, on);
      }
    }
  }

  void fillCircle(int cx, int cy, int r, bool on) override {
    for (int y = -r; y <= r; ++y)
      for (int x = -r; x <= r; ++x)
        if (x * x + y * y <= r * r + r) set(cx + x, cy + y, on);
  }

  void drawText(int x, int y, const char* text, int scale, bool on) override {
    for (size_t i = 0; text[i] != '\0'; ++i) {
      int c = static_cast<unsigned char>(text[i]);
      if (c < kFirst() || c > kLast()) c = '?';
      const unsigned char* glyph = hosttest::kFont5x7[c - kFirst()];
      for (int col = 0; col < 5; ++col) {
        for (int row = 0; row < 8; ++row) {
          if (((glyph[col] >> row) & 1) == 0) continue;
          fillRect(x + (static_cast<int>(i) * motonav::kGlyphCellW + col) * scale,
                   y + row * scale, scale, scale, on);
        }
      }
    }
  }

  void present() override { ++presentCount_; }

  // --- inspection ---
  bool pixel(int x, int y) const { return px_[static_cast<size_t>(y) * w_ + x] != 0; }
  int inkCount() const {
    int n = 0;
    for (uint8_t p : px_) n += p;
    return n;
  }
  int inkIn(int x0, int y0, int x1, int y1) const {  // [x0,x1) x [y0,y1)
    int n = 0;
    for (int y = std::max(0, y0); y < std::min(h_, y1); ++y)
      for (int x = std::max(0, x0); x < std::min(w_, x1); ++x) n += pixel(x, y);
    return n;
  }
  int outOfBounds() const { return outOfBounds_; }
  int presentCount() const { return presentCount_; }
  void resetCounters() { outOfBounds_ = 0; presentCount_ = 0; }

  // FNV-1a over size and pixels. Stable across platforms.
  uint64_t hash() const {
    uint64_t h = 1469598103934665603ULL;
    auto mix = [&h](uint8_t b) { h ^= b; h *= 1099511628211ULL; };
    mix(static_cast<uint8_t>(w_)); mix(static_cast<uint8_t>(h_));
    for (uint8_t p : px_) mix(p);
    return h;
  }

  // Plain-text PBM ("P1"), viewable by most image tools.
  bool writePbm(const std::string& path) const {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fprintf(f, "P1\n%d %d\n", w_, h_);
    for (int y = 0; y < h_; ++y) {
      for (int x = 0; x < w_; ++x) fputc(pixel(x, y) ? '1' : '0', f);
      fputc('\n', f);
    }
    fclose(f);
    return true;
  }

  // For debugging in a terminal.
  std::string ascii() const {
    std::string s;
    for (int y = 0; y < h_; ++y) {
      for (int x = 0; x < w_; ++x) s += pixel(x, y) ? '#' : '.';
      s += '\n';
    }
    return s;
  }

 private:
  static int kFirst() { return hosttest::kFontFirst; }
  static int kLast() { return hosttest::kFontLast; }
  static long edge(int ax, int ay, int bx, int by, int px, int py) {
    return static_cast<long>(bx - ax) * (py - ay) - static_cast<long>(by - ay) * (px - ax);
  }
  void set(int x, int y, bool on) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) {
      ++outOfBounds_;
      return;
    }
    px_[static_cast<size_t>(y) * w_ + x] = on ? 1 : 0;
  }

  int w_, h_;
  std::vector<uint8_t> px_;
  int outOfBounds_ = 0;
  int presentCount_ = 0;
};

}  // namespace hosttest
