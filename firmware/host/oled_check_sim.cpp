// Runs the real firmware/src/tools/oled_check.cpp against a bitmap "panel" and
// writes each test pattern as a PBM image: what a HEALTHY SSD1306 should show.
//   ./build/oled_check_sim <output directory>
// HOST ONLY. This is a prediction from the drawing code, not a photograph.
#include <stdio.h>
#include <sys/stat.h>

#include <string>
#include <vector>

#include "Adafruit_GFX.h"
#include "Arduino.h"

static void writePbm(const std::string& path, const Adafruit_SSD1306& d) {
  FILE* f = fopen(path.c_str(), "w");
  if (!f) { fprintf(stderr, "cannot write %s\n", path.c_str()); return; }
  fprintf(f, "P1\n%d %d\n", d.width(), d.height());
  for (int y = 0; y < d.height(); ++y) {
    for (int x = 0; x < d.width(); ++x) fputc(d.pixel(x, y) ? '1' : '0', f);
    fputc('\n', f);
  }
  fclose(f);
}

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : "out";
  mkdir(dir.c_str(), 0755);

  int frame = 0;
  Adafruit_SSD1306::displayHook() = [&](const Adafruit_SSD1306& d) {
    ++frame;
    writePbm(dir + "/oled_check_pattern_" + std::to_string(frame) + ".pbm", d);
  };

  setup();                       // shows pattern 1
  for (char key = '2'; key <= '5'; ++key) {
    Serial.input.push_back(key);  // "type" the key into the serial monitor
    loop();                       // shows the requested pattern
  }
  fputs(Serial.out.c_str(), stdout);
  printf("(%d frames written to %s/)\n", frame, dir.c_str());
  return frame == 5 ? 0 : 1;
}
