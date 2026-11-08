// MotoNav firmware, Phase 1: shows simulated navigation on the display.
// No Bluetooth yet. This file is only wiring: hardware setup, a loop that asks
// the simulator for a state, and the Presenter that decides when to redraw.
//
// It also prints the log lines used to judge long runs (boot, reset reason, a
// stats line every minute, render-consistency checks). Their exact text is
// defined in lib/motonav/src/diag/log_format.* and analysed by
// firmware/host/analyze_log.py.
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>

// The prototype target is the classic ESP32 (see HARDWARE.md). Building for any
// other chip stops here instead of producing a binary nobody has planned for.
#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "MotoNav firmware targets the classic ESP32. Use the esp32_nodemcu environment, or see HARDWARE.md if the board turns out to be a different chip."
#endif

#include "diag/frame_check.h"
#include "diag/log_format.h"
#include "display_config.h"
#include "sim/sim.h"
#include "ssd1306_canvas.h"
#include "ui/presenter.h"
#include "ui/view.h"

using namespace motonav;

namespace {

constexpr uint32_t kInitRetryMs = 2000;
constexpr uint32_t kStatsEveryMs = 60000;
constexpr size_t kFrameBytes = (MOTONAV_OLED_WIDTH * MOTONAV_OLED_HEIGHT) / 8;

Adafruit_SSD1306 oled(MOTONAV_OLED_WIDTH, MOTONAV_OLED_HEIGHT, &Wire, -1);
Ssd1306Canvas canvas(oled);
Presenter presenter(canvas);
SimController sim;
FrameConsistency consistency;

bool displayReady = false;
uint32_t lastInitTryMs = 0;
uint32_t lastStatsMs = 0;
uint32_t framesPresented = 0;
uint32_t presentMaxMs = 0;  // longest display update since the last stats line

const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "POWERON";    // power applied, or the EN button / upload reset
    case ESP_RST_EXT:       return "EXT";        // reset pin
    case ESP_RST_SW:        return "SW";         // software restart
    case ESP_RST_PANIC:     return "PANIC";      // crash
    case ESP_RST_INT_WDT:   return "INT_WDT";    // interrupt watchdog
    case ESP_RST_TASK_WDT:  return "TASK_WDT";   // task watchdog: the program stopped responding
    case ESP_RST_WDT:       return "WDT";        // other watchdog
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";   // supply voltage dipped: weak USB power or cable
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "UNKNOWN";
  }
}

// Prints every I2C address that answers. Helps identify an unknown display.
void scanI2c() {
  Serial.println("I2C scan:");
  int found = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  device at 0x%02X\n", address);
      ++found;
    }
  }
  if (found == 0) Serial.println("  nothing answered (check wiring, power and pin numbers)");
}

bool answers(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

// Adafruit's begin() does not notice an absent display, so probe it ourselves.
bool initDisplay() {
  Wire.beginTransmission(MOTONAV_OLED_I2C_ADDR);
  const uint8_t error = Wire.endTransmission();
  if (error != 0) {
    Serial.printf("display: no answer at 0x%02X (I2C error %u)\n",
                  static_cast<unsigned>(MOTONAV_OLED_I2C_ADDR), static_cast<unsigned>(error));
    // The two usual OLED addresses are 0x3C and 0x3D. Say so if it is at the other one.
    const uint8_t other = MOTONAV_OLED_I2C_ADDR == 0x3C ? 0x3D : 0x3C;
    if (answers(other)) {
      Serial.printf("display: but a device DOES answer at 0x%02X. Set -DMOTONAV_OLED_I2C_ADDR=0x%02X "
                    "in firmware/platformio.ini and upload again.\n",
                    static_cast<unsigned>(other), static_cast<unsigned>(other));
    }
    return false;
  }
  // periphBegin=false: we already started Wire on our own pins.
  if (!oled.begin(SSD1306_SWITCHCAPVCC, MOTONAV_OLED_I2C_ADDR, true, false)) {
    Serial.println("display: SSD1306 begin() failed (frame buffer allocation)");
    return false;
  }
  Serial.println("display: initialised");
  presenter.invalidate();  // the panel content is unknown: force a full draw
  return true;
}

// Prints what the display should be showing right now, so a tester can compare
// the screen with the text.
void printExpectation(uint32_t now) {
  char line[160];
  describeView(buildView(sim.state(now)), line, sizeof(line));
  Serial.printf("expect: %s\n", line);
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c == '\r' || c == '\n') continue;
    const uint32_t now = millis();
    if (sim.handleCommand(static_cast<char>(c), now)) {
      Serial.printf("sim: command '%c' (%s)\n", c, sim.isAuto() ? "auto" : "hold");
      if (!sim.isAuto()) printExpectation(now);
    } else {
      Serial.printf("sim: unknown command '%c'.\n", c);
      Serial.println("sim: a=auto  0=disconnected 1=ready 2=navigating 3=rerouting 4=arrived");
      Serial.println("sim: n=next maneuver  d=next distance  r=next road name");
    }
  }
}

void printStats(uint32_t now) {
  StatsSnapshot s;
  s.uptimeSeconds = now / 1000UL;
  s.heapFree = ESP.getFreeHeap();
  s.heapMin = ESP.getMinFreeHeap();
  s.maxAlloc = ESP.getMaxAllocHeap();
  s.stackFree = uxTaskGetStackHighWaterMark(nullptr);
  s.frames = framesPresented;
  s.presentMaxMs = presentMaxMs;
  s.renderMismatches = consistency.mismatches();
  presentMaxMs = 0;

  char line[200];
  formatStatsLine(line, sizeof(line), s);
  Serial.println(line);
}

// Screens that never change must produce the same frame buffer every time.
void checkRenderConsistency(const DeviceState& state) {
  const ScreenView view = buildView(state);
  if (view.screen == Screen::Navigating) return;  // content varies with the data
  const int key = static_cast<int>(view.screen);
  const uint32_t hash = fnv1a32(oled.getBuffer(), kFrameBytes);
  const uint32_t reference = consistency.reference(key);
  const FrameCheck result = consistency.check(key, hash);
  if (result == FrameCheck::Match) return;
  char line[120];
  formatRenderLine(line, sizeof(line), result, screenName(view.screen), hash, reference);
  Serial.println(line);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();

  char line[100];
  formatBootLine(line, sizeof(line));
  Serial.println(line);
  formatResetLine(line, sizeof(line), resetReasonName(esp_reset_reason()));
  Serial.println(line);

  Serial.printf("chip: %s rev %u, %u core(s), flash %lu bytes\n", ESP.getChipModel(),
                static_cast<unsigned>(ESP.getChipRevision()),
                static_cast<unsigned>(ESP.getChipCores()),
                static_cast<unsigned long>(ESP.getFlashChipSize()));
  Serial.printf("config: %dx%d display at 0x%02X, SDA=%d SCL=%d\n",
                MOTONAV_OLED_WIDTH, MOTONAV_OLED_HEIGHT,
                static_cast<unsigned>(MOTONAV_OLED_I2C_ADDR), MOTONAV_I2C_SDA, MOTONAV_I2C_SCL);

  Wire.begin(MOTONAV_I2C_SDA, MOTONAV_I2C_SCL);
  scanI2c();
  displayReady = initDisplay();
  lastInitTryMs = millis();
}

void loop() {
  const uint32_t now = millis();

  // A failed init is reported and retried; it never ends in a reboot loop.
  if (!displayReady) {
    if (now - lastInitTryMs >= kInitRetryMs) {
      lastInitTryMs = now;
      displayReady = initDisplay();
    }
    delay(20);
    return;
  }

  handleSerialCommands();

  const DeviceState state = sim.state(now);
  const uint32_t before = millis();
  if (presenter.update(state)) {
    const uint32_t took = millis() - before;
    if (took > presentMaxMs) presentMaxMs = took;
    ++framesPresented;
    checkRenderConsistency(state);
  }

  if (now - lastStatsMs >= kStatsEveryMs) {
    lastStatsMs = now;
    printStats(now);
  }
  delay(10);
}
