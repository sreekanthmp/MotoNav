// OLED controller check: draws test patterns that tell an SSD1306 from an
// SH1106 and show wiring or panel faults.
//
// Standalone diagnostic, not the MotoNav firmware. Build and flash with:
//   pio run -e oled_check -t upload      then      pio device monitor
//
// Why this works: the library drives the panel as an SSD1306 (128 columns). An
// SH1106 has 132 columns of memory and shows columns 2..129. Driven as an
// SSD1306 it therefore (a) cuts off the first two columns on the left and
// (b) shows two columns of random memory on the right. The patterns below make
// both easy to see. See HARDWARE.md ("Verifying the hardware") for how to read
// them.
//
// Patterns change every 6 seconds. Type 1, 2 or 3 on the serial monitor to
// hold one, or 'a' to cycle again.
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#include "display_config.h"

namespace {

constexpr uint32_t kPatternMs = 6000;
constexpr int kPatternCount = 3;

Adafruit_SSD1306 oled(MOTONAV_OLED_WIDTH, MOTONAV_OLED_HEIGHT, &Wire, -1);

bool displayReady = false;
bool cycling = true;
int pattern = 0;
uint32_t patternStartMs = 0;

// Pattern 1: a one-pixel border on the very edge of the frame, diagonals and
// edge labels. A correct SSD1306 shows all four sides of the border.
void drawBorderPattern() {
  const int w = oled.width();
  const int h = oled.height();
  oled.clearDisplay();
  oled.drawRect(0, 0, w, h, SSD1306_WHITE);   // outermost pixels
  oled.drawLine(0, 0, w - 1, h - 1, SSD1306_WHITE);
  oled.drawLine(0, h - 1, w - 1, 0, SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  oled.setCursor(w / 2 - 33, 8);
  oled.print(" BORDER TEST ");
  oled.setCursor(w / 2 - 33, h - 16);
  oled.print(" 4 SIDES? ");
  oled.display();
}

// Pattern 2: every pixel on. A correct SSD1306 is solid white to all four
// edges. An SH1106 driven as an SSD1306 shows a dark or speckled strip about
// two pixels wide on the right edge.
void drawAllOnPattern() {
  oled.clearDisplay();
  oled.fillRect(0, 0, oled.width(), oled.height(), SSD1306_WHITE);
  oled.display();
}

// Pattern 3: single-pixel vertical lines at x = 0, 1, 2, 3 and at the last four
// columns, plus a pixel ruler along the top. Shows exactly which edge columns
// are visible.
void drawEdgeColumnsPattern() {
  const int w = oled.width();
  const int h = oled.height();
  oled.clearDisplay();
  for (int i = 0; i < 4; ++i) {
    oled.drawFastVLine(i * 2, 0, h, SSD1306_WHITE);           // x = 0, 2, 4, 6
    oled.drawFastVLine(w - 1 - i * 2, 0, h, SSD1306_WHITE);   // x = 127, 125, 123, 121
  }
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  oled.setCursor(18, 28);
  oled.print("EDGE COLUMNS");
  oled.display();
}

void showPattern() {
  switch (pattern) {
    case 0: drawBorderPattern(); break;
    case 1: drawAllOnPattern(); break;
    default: drawEdgeColumnsPattern(); break;
  }
  Serial.printf("pattern %d of %d: %s\n", pattern + 1, kPatternCount,
                pattern == 0 ? "border (expect all four sides)"
                : pattern == 1 ? "all pixels on (expect solid white to every edge)"
                               : "edge columns (expect 4 lines at each side, 2 px apart)");
}

void handleSerial() {
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c >= '1' && c <= '3') {
      cycling = false;
      pattern = c - '1';
      showPattern();
    } else if (c == 'a') {
      cycling = true;
      patternStartMs = millis();
      Serial.println("cycling patterns");
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("MotoNav oled_check (diagnostic, not the MotoNav firmware)");
  Serial.printf("config: %dx%d at 0x%02X, SDA=%d SCL=%d\n", MOTONAV_OLED_WIDTH, MOTONAV_OLED_HEIGHT,
                static_cast<unsigned>(MOTONAV_OLED_I2C_ADDR), MOTONAV_I2C_SDA, MOTONAV_I2C_SCL);

  Wire.begin(MOTONAV_I2C_SDA, MOTONAV_I2C_SCL);
  Wire.beginTransmission(MOTONAV_OLED_I2C_ADDR);
  const uint8_t error = Wire.endTransmission();
  if (error != 0) {
    Serial.printf("NO ANSWER at 0x%02X (I2C error %u). Run hw_check first to find the address.\n",
                  static_cast<unsigned>(MOTONAV_OLED_I2C_ADDR), static_cast<unsigned>(error));
    return;
  }
  if (!oled.begin(SSD1306_SWITCHCAPVCC, MOTONAV_OLED_I2C_ADDR, true, false)) {
    Serial.println("SSD1306 begin() failed (frame buffer allocation)");
    return;
  }
  displayReady = true;
  Serial.println("Driving the panel as an SSD1306. Keys: 1/2/3 hold a pattern, a = cycle.");
  showPattern();
  patternStartMs = millis();
}

void loop() {
  if (!displayReady) {
    delay(100);
    return;
  }
  handleSerial();
  if (cycling && millis() - patternStartMs >= kPatternMs) {
    patternStartMs = millis();
    pattern = (pattern + 1) % kPatternCount;
    showPattern();
  }
  delay(20);
}
