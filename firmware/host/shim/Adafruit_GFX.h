// HOST-ONLY stand-in for the parts of Adafruit GFX / SSD1306 that oled_check.cpp
// uses, drawing into a plain bitmap. It exists to render the diagnostic's test
// patterns as they should look on a healthy SSD1306. Edge pixels can differ by
// about one pixel from the real library.
#pragma once

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <vector>

#include "Arduino.h"
#include "../font5x7.h"

constexpr uint16_t SSD1306_BLACK = 0;
constexpr uint16_t SSD1306_WHITE = 1;
constexpr uint8_t SSD1306_SWITCHCAPVCC = 0x02;

class Adafruit_SSD1306 {
 public:
  Adafruit_SSD1306(uint8_t w, uint8_t h, void*, int8_t) : w_(w), h_(h), px_(static_cast<size_t>(w) * h, 0) {}

  bool begin(uint8_t, uint8_t, bool = true, bool = true) { return true; }
  int16_t width() const { return w_; }
  int16_t height() const { return h_; }

  void clearDisplay() { std::fill(px_.begin(), px_.end(), 0); }
  void display() {
    if (displayHook()) displayHook()(*this);
  }
  // Test drivers set this to capture every frame the firmware sends to the panel.
  static std::function<void(const Adafruit_SSD1306&)>& displayHook() {
    static std::function<void(const Adafruit_SSD1306&)> hook;
    return hook;
  }

  void drawPixel(int x, int y, uint16_t c) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    px_[static_cast<size_t>(y) * w_ + x] = c ? 1 : 0;
  }
  void drawFastHLine(int x, int y, int w, uint16_t c) { for (int i = 0; i < w; ++i) drawPixel(x + i, y, c); }
  void drawFastVLine(int x, int y, int h, uint16_t c) { for (int i = 0; i < h; ++i) drawPixel(x, y + i, c); }
  void drawRect(int x, int y, int w, int h, uint16_t c) {
    drawFastHLine(x, y, w, c);
    drawFastHLine(x, y + h - 1, w, c);
    drawFastVLine(x, y, h, c);
    drawFastVLine(x + w - 1, y, h, c);
  }
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    for (int j = 0; j < h; ++j) drawFastHLine(x, y + j, w, c);
  }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {  // Bresenham
    const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
      drawPixel(x0, y0, c);
      if (x0 == x1 && y0 == y1) break;
      const int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }

  void setTextSize(uint8_t s) { size_ = s; }
  void setTextColor(uint16_t fg) { fg_ = fg; bg_ = fg; }
  void setTextColor(uint16_t fg, uint16_t bg) { fg_ = fg; bg_ = bg; }
  void setTextWrap(bool) {}
  void setCursor(int x, int y) { cx_ = x; cy_ = y; }
  void print(const char* text) {
    for (; *text; ++text) {
      int c = static_cast<unsigned char>(*text);
      if (c < hosttest::kFontFirst || c > hosttest::kFontLast) c = '?';
      if (bg_ != fg_) fillRect(cx_, cy_, 6 * size_, 8 * size_, bg_);
      const unsigned char* glyph = hosttest::kFont5x7[c - hosttest::kFontFirst];
      for (int col = 0; col < 5; ++col)
        for (int row = 0; row < 8; ++row)
          if ((glyph[col] >> row) & 1) fillRect(cx_ + col * size_, cy_ + row * size_, size_, size_, fg_);
      cx_ += 6 * size_;
    }
  }

  int pixel(int x, int y) const { return px_[static_cast<size_t>(y) * w_ + x]; }

 private:
  int w_, h_;
  std::vector<uint8_t> px_;
  int size_ = 1;
  uint16_t fg_ = 1, bg_ = 1;
  int cx_ = 0, cy_ = 0;
};
