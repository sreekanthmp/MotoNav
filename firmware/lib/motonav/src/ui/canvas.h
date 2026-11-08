// The only thing the screens draw through.
//
// A canvas is a small drawing surface. The SSD1306 implements it on the
// device (src/ssd1306_canvas.*), a plain bitmap implements it in the host
// tests, and a circular AMOLED will implement it later. Nothing above this
// line knows which one it is talking to.
//
// Colour is one bit for now (on/off). A colour display will extend this.
#pragma once

#include <stddef.h>
#include <string.h>

namespace motonav {

// Text uses a fixed 6x8 cell per character at scale 1 (5x7 glyph plus one
// column and one row of spacing), which is the Adafruit GFX default font.
// All backends must use the same cell so layouts are identical everywhere.
constexpr int kGlyphCellW = 6;
constexpr int kGlyphCellH = 8;

// Width in pixels of the ink of `text` at `scale` (excludes the trailing
// spacing column). Zero for empty text.
inline int textWidth(const char* text, int scale) {
  const size_t n = strlen(text);
  return n == 0 ? 0 : static_cast<int>(n) * kGlyphCellW * scale - scale;
}

struct Box {
  int x;
  int y;
  int w;
  int h;
};

class Canvas {
 public:
  virtual ~Canvas() {}

  virtual int width() const = 0;
  virtual int height() const = 0;

  virtual void clear() = 0;  // every pixel off
  virtual void fillRect(int x, int y, int w, int h, bool on) = 0;
  virtual void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2,
                            bool on) = 0;
  virtual void fillCircle(int cx, int cy, int r, bool on) = 0;
  // (x, y) is the top-left of the first character cell.
  virtual void drawText(int x, int y, const char* text, int scale, bool on) = 0;

  // Sends the finished frame to the panel. The only call that talks to
  // hardware; screens never call it. See Presenter.
  virtual void present() = 0;
};

}  // namespace motonav
