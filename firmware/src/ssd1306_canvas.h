// Canvas implementation for an SSD1306 panel, using Adafruit GFX.
//
// This is the only file (with main.cpp) that knows an SSD1306 exists. A
// different display gets its own small adapter like this one; the screens do
// not change.
#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "ui/canvas.h"

class Ssd1306Canvas : public motonav::Canvas {
 public:
  explicit Ssd1306Canvas(Adafruit_SSD1306& display) : display_(display) {}

  int width() const override { return display_.width(); }
  int height() const override { return display_.height(); }

  void clear() override { display_.clearDisplay(); }

  void fillRect(int x, int y, int w, int h, bool on) override {
    display_.fillRect(x, y, w, h, color(on));
  }

  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool on) override {
    display_.fillTriangle(x0, y0, x1, y1, x2, y2, color(on));
  }

  void fillCircle(int cx, int cy, int r, bool on) override {
    display_.fillCircle(cx, cy, r, color(on));
  }

  void drawText(int x, int y, const char* text, int scale, bool on) override {
    display_.setTextSize(scale);
    display_.setTextColor(color(on));
    display_.setTextWrap(false);
    display_.setCursor(x, y);
    display_.print(text);
  }

  // Sends the whole frame buffer to the panel over I2C.
  void present() override { display_.display(); }

 private:
  static uint16_t color(bool on) { return on ? SSD1306_WHITE : SSD1306_BLACK; }

  Adafruit_SSD1306& display_;
};
