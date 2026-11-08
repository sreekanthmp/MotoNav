// MotoNav firmware, Phase 2: shows navigation messages received over Bluetooth LE.
//
// STATUS: compile-checked only. Never run on a real board. The Phase 1 firmware
// (main.cpp, environment `motonav`) is untouched; this is the separate environment
// `motonav_ble`.
//
//   phone --BLE--> BleLink --queue--> Receiver (host-tested) --> DeviceState --> Presenter --> OLED
//
// The serial commands of Phase 1 still work: any of them switches the display to the
// built-in simulation, and `b` switches back to Bluetooth.
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "MotoNav firmware targets the classic ESP32. See HARDWARE.md if the board turns out to be a different chip."
#endif

#include "ble_link.h"
#include "device_common.h"
#include "diag/frame_check.h"
#include "diag/log_format.h"
#include "display_config.h"
#include "protocol/ble_log.h"
#include "protocol/receiver.h"
#include "sim/sim.h"
#include "ssd1306_canvas.h"
#include "ui/presenter.h"
#include "ui/view.h"

using namespace motonav;

namespace {

constexpr uint32_t kInitRetryMs = 2000;
constexpr uint32_t kStatsEveryMs = 60000;
constexpr uint32_t kRejectLogGapMs = 100;  // at most ten rejection lines a second
constexpr size_t kFrameBytes = (MOTONAV_OLED_WIDTH * MOTONAV_OLED_HEIGHT) / 8;

enum class Source { Ble, Sim };

Adafruit_SSD1306 oled(MOTONAV_OLED_WIDTH, MOTONAV_OLED_HEIGHT, &Wire, -1);
Ssd1306Canvas canvas(oled);
Presenter presenter(canvas);
SimController sim;
FrameConsistency consistency;
Receiver receiver;
BleLink ble;

Source source = Source::Ble;
bool bleStarted = false;
bool displayReady = false;
uint32_t lastInitTryMs = 0;
uint32_t lastStatsMs = 0;
uint32_t lastRejectLogMs = 0;
uint32_t framesPresented = 0;
uint32_t presentMaxMs = 0;
uint16_t lastMtu = 23;
uint32_t seenOverflows = 0;
uint32_t seenOversizes = 0;

// Adafruit's begin() does not notice an absent display, so probe it ourselves.
bool initDisplay() {
  Wire.beginTransmission(MOTONAV_OLED_I2C_ADDR);
  const uint8_t error = Wire.endTransmission();
  if (error != 0) {
    Serial.printf("display: no answer at 0x%02X (I2C error %u)\n",
                  static_cast<unsigned>(MOTONAV_OLED_I2C_ADDR), static_cast<unsigned>(error));
    const uint8_t other = MOTONAV_OLED_I2C_ADDR == 0x3C ? 0x3D : 0x3C;
    if (i2cAnswers(other)) {
      Serial.printf("display: but a device DOES answer at 0x%02X. Set -DMOTONAV_OLED_I2C_ADDR=0x%02X "
                    "in firmware/platformio.ini and upload again.\n",
                    static_cast<unsigned>(other), static_cast<unsigned>(other));
    }
    return false;
  }
  if (!oled.begin(SSD1306_SWITCHCAPVCC, MOTONAV_OLED_I2C_ADDR, true, false)) {
    Serial.println("display: SSD1306 begin() failed (frame buffer allocation)");
    return false;
  }
  Serial.println("display: initialised");
  presenter.invalidate();
  return true;
}

DeviceState currentState(uint32_t now) {
  if (source == Source::Sim) return sim.state(now);
  return deviceStateFrom(ble.connected(), receiver);
}

void printExpectation(uint32_t now) {
  char line[160];
  describeView(buildView(currentState(now)), line, sizeof(line));
  Serial.printf("expect: %s\n", line);
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c == '\r' || c == '\n') continue;
    const uint32_t now = millis();
    if (c == 'b') {
      source = Source::Ble;
      Serial.println("source: ble");
      printExpectation(now);
    } else if (sim.handleCommand(static_cast<char>(c), now)) {
      source = Source::Sim;
      Serial.printf("source: sim, command '%c' (%s)\n", c, sim.isAuto() ? "auto" : "hold");
      if (!sim.isAuto()) printExpectation(now);
    } else {
      Serial.printf("unknown command '%c'.\n", c);
      Serial.println("b=bluetooth (default)   simulation: a=auto 0..4=hold screen n=next maneuver d=next distance r=next road");
    }
  }
}

// Connection events, queued writes, and counters for writes lost before they could be parsed.
void serviceBle(uint32_t now) {
  char line[160];
  bool statusDirty = false;

  int reason = 0;
  if (ble.takeDisconnected(&reason)) {
    receiver.reset();
    ble.flush();
    formatBleLinkLine(line, sizeof(line), false, reason);
    Serial.println(line);
    lastMtu = 23;
    statusDirty = true;
  }
  if (ble.takeConnected()) {
    receiver.reset();  // sequence numbers restart with every connection
    ble.flush();
    formatBleLinkLine(line, sizeof(line), true, 0);
    Serial.println(line);
    statusDirty = true;
  }
  if (ble.mtu() != lastMtu) {
    lastMtu = ble.mtu();
    formatBleMtuLine(line, sizeof(line), lastMtu);
    Serial.println(line);
  }

  while (seenOverflows < ble.overflowCount()) {
    ++seenOverflows;
    receiver.recordDropped(ProtocolError::QueueOverflow);
    statusDirty = true;
  }
  while (seenOversizes < ble.oversizeCount()) {
    ++seenOversizes;
    receiver.recordDropped(ProtocolError::BadLength);
    statusDirty = true;
  }

  uint8_t frame[kMaxFrameBytes];
  size_t length = 0;
  while (ble.popFrame(frame, sizeof(frame), &length)) {
    const RxResult result = receiver.receive(frame, length);
    statusDirty = true;
    if (result == RxResult::Accepted) {
      // Say what the display should now show, so a tester can compare screen and text.
      char view[160];
      describeView(buildView(deviceStateFrom(ble.connected(), receiver)), view, sizeof(view));
      char accepted[240];
      formatBleAcceptedLine(accepted, sizeof(accepted), receiver.stats().lastAcceptedSequence, view);
      Serial.println(accepted);
    } else if (now - lastRejectLogMs >= kRejectLogGapMs) {
      lastRejectLogMs = now;
      formatBleRejectLine(line, sizeof(line), result, receiver.lastCallError(), length,
                          length > 2 ? frame[2] : 0);
      Serial.println(line);
    }
  }

  if (statusDirty) {
    uint8_t status[kStatusBytes];
    encodeStatus(receiver.stats(), status);
    ble.publishStatus(status, kStatusBytes);
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

  char line[220];
  formatStatsLine(line, sizeof(line), s);
  Serial.println(line);
  formatBleStatsLine(line, sizeof(line), ble.connected(), receiver.stats(),
                     ble.overflowCount() + ble.oversizeCount());
  Serial.println(line);
}

void checkRenderConsistency(const DeviceState& state) {
  const ScreenView view = buildView(state);
  if (view.screen == Screen::Navigating) return;
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
  formatBleBootLine(line, sizeof(line));
  Serial.println(line);
  formatResetLine(line, sizeof(line), resetReasonName(esp_reset_reason()));
  Serial.println(line);

  Serial.printf("chip: %s rev %u, %u core(s), flash %lu bytes\n", ESP.getChipModel(),
                static_cast<unsigned>(ESP.getChipRevision()),
                static_cast<unsigned>(ESP.getChipCores()),
                static_cast<unsigned long>(ESP.getFlashChipSize()));
  Serial.printf("config: %dx%d display at 0x%02X, SDA=%d SCL=%d\n", MOTONAV_OLED_WIDTH,
                MOTONAV_OLED_HEIGHT, static_cast<unsigned>(MOTONAV_OLED_I2C_ADDR), MOTONAV_I2C_SDA,
                MOTONAV_I2C_SCL);

  Wire.begin(MOTONAV_I2C_SDA, MOTONAV_I2C_SCL);
  scanI2c();
  displayReady = initDisplay();
  lastInitTryMs = millis();

  bleStarted = ble.begin();
  if (bleStarted) {
    Serial.println("ble: advertising as MotoNav");
  } else {
    Serial.println("ble: FAILED to start. The display still works; use the simulation commands.");
  }
}

void loop() {
  const uint32_t now = millis();

  // Bluetooth is serviced even if the display is missing, so the link and counters still work.
  if (bleStarted) serviceBle(now);
  handleSerialCommands();

  if (!displayReady) {
    if (now - lastInitTryMs >= kInitRetryMs) {
      lastInitTryMs = now;
      displayReady = initDisplay();
    }
    delay(20);
    return;
  }

  const DeviceState state = currentState(now);
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
