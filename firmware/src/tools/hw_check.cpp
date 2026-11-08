// Hardware check: identifies the chip and board, and tests the I2C bus.
//
// A standalone diagnostic, not the MotoNav firmware. It needs no display
// library, so a wiring problem cannot hide behind a driver problem.
//   pio run -e hw_check -t upload      then      pio device monitor
//
// It READS the hardware. The decisions (is this a pass?) are made by the pure,
// host-tested functions in lib/motonav/src/diag/hw_verdict.*.
//
// What it checks, in order:
//   1. chip model, radios (BLE is required), cores and clock, flash size read
//      from the flash chip itself
//   2. that the configured SDA and SCL pins are usable GPIOs
//   3. the electrical state of both I2C lines (short to ground? pull-up?)
//   4. every I2C address that answers, at 100 kHz and 400 kHz
//   5. 200 repeated probes of the OLED at each speed (the firmware uses 400 kHz)
//   6. if nothing answers, a second scan with SDA and SCL swapped
// It repeats every 5 seconds so wires can be moved while it runs. After the
// first full report it stays quiet while nothing changes.
//
// See HARDWARE.md and TESTING.md for how to read the output.
#include <Arduino.h>
#include <Wire.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <stdarg.h>
#include <stdio.h>

#include "diag/hw_verdict.h"
#include "display_config.h"  // MOTONAV_I2C_SDA, MOTONAV_I2C_SCL, MOTONAV_OLED_I2C_ADDR

using namespace motonav;

namespace {

constexpr uint32_t kRepeatMs = 5000;
constexpr int kProbeCount = 200;
constexpr uint32_t kSlowClockHz = 100000;
constexpr uint32_t kFastClockHz = 400000;  // the OLED driver in the firmware runs at this speed
constexpr int kMaxFound = 16;
constexpr int kMaxItems = 24;

struct Item {
  const char* name;
  Finding finding;
};

struct Summary {
  Verdict overall = Verdict::Pass;
  int pass = 0;
  int warn = 0;
  int fail = 0;
  int oledAddress = -1;
  uint8_t foundMask[16] = {0};  // addresses seen at 100 kHz, one bit each
};

bool verbose = true;
Item items[kMaxItems];
int itemCount = 0;
Summary summary;

uint32_t lastRunMs = 0;
uint32_t runCount = 0;
bool haveLast = false;
Summary last;
uint32_t sameAsPreviousRuns = 0;

// Prints only when the current run is verbose.
void say(const char* format, ...) __attribute__((format(printf, 1, 2)));
void say(const char* format, ...) {
  if (!verbose) return;
  char line[200];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  Serial.println(line);
}

void report(const char* name, const Finding& finding) {
  say("[%s] %s: %s", verdictLabel(finding.verdict), name, finding.text);
  if (itemCount < kMaxItems) items[itemCount++] = Item{name, finding};
  summary.overall = worseOf(summary.overall, finding.verdict);
  if (finding.verdict == Verdict::Pass) ++summary.pass;
  if (finding.verdict == Verdict::Warn) ++summary.warn;
  if (finding.verdict == Verdict::Fail) ++summary.fail;
}

const char* modelName(esp_chip_model_t model) {
  switch (model) {
    case CHIP_ESP32:   return "ESP32 (classic)";
    case CHIP_ESP32S2: return "ESP32-S2";
    case CHIP_ESP32S3: return "ESP32-S3";
    case CHIP_ESP32C3: return "ESP32-C3";
    default:           return "other / unknown";
  }
}

void setBit(uint8_t* map, uint8_t address) { map[address >> 3] |= static_cast<uint8_t>(1u << (address & 7)); }

bool sameMask(const uint8_t* a, const uint8_t* b) {
  for (int i = 0; i < 16; ++i) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

// --------------------------------- chip -------------------------------------

void checkChipAndBoard() {
  esp_chip_info_t info;
  esp_chip_info(&info);

  say("--- Chip and board ---");
  say("Silicon model    : %s", modelName(info.model));
  say("Model (SDK name) : %s", ESP.getChipModel());
  say("Revision         : %u", static_cast<unsigned>(ESP.getChipRevision()));
  say("CPU              : %u core(s) at %lu MHz", static_cast<unsigned>(info.cores),
      static_cast<unsigned long>(ESP.getCpuFreqMHz()));
  say("Radios           : %s%s%s", (info.features & CHIP_FEATURE_WIFI_BGN) ? "Wi-Fi " : "",
      (info.features & CHIP_FEATURE_BLE) ? "BLE " : "",
      (info.features & CHIP_FEATURE_BT) ? "Bluetooth-Classic" : "");
  say("MAC (eFuse)      : %012llX", static_cast<unsigned long long>(ESP.getEfuseMac()));

  // Flash size from the flash chip's own JEDEC id: manufacturer, type, capacity.
  uint32_t jedec = 0;
  uint32_t physical = 0;
  if (esp_flash_read_id(nullptr, &jedec) == ESP_OK) {
    physical = flashBytesFromJedecCapacity(static_cast<uint8_t>(jedec & 0xFF));
    say("Flash JEDEC id   : 0x%06lX (manufacturer 0x%02lX, capacity code 0x%02lX)",
        static_cast<unsigned long>(jedec), static_cast<unsigned long>((jedec >> 16) & 0xFF),
        static_cast<unsigned long>(jedec & 0xFF));
  } else {
    say("Flash JEDEC id   : could not be read");
  }
  say("Flash from chip  : %lu bytes", static_cast<unsigned long>(physical));
  say("Flash per SDK    : %lu bytes", static_cast<unsigned long>(ESP.getFlashChipSize()));

  report("chip", judgeChip(info.model == CHIP_ESP32));
  report("radios", judgeRadios((info.features & CHIP_FEATURE_WIFI_BGN) != 0,
                               (info.features & CHIP_FEATURE_BLE) != 0));
  report("cpu", judgeCores(info.cores, ESP.getCpuFreqMHz()));
  report("flash", judgeFlashSize(physical, ESP.getFlashChipSize()));
}

// ------------------------------ pins and lines ------------------------------

bool readLevel(int pin, int mode) {
  pinMode(pin, mode);
  delay(5);
  return digitalRead(pin) != 0;
}

// Reads one I2C line with the internal pull-up, then the internal pull-down.
// Only done for pins that are safe to touch and have internal pulls.
void checkLine(const char* name, int pin) {
  if (judgeI2cPin(pin).verdict == Verdict::Fail) {
    say("%s (GPIO%d): line test skipped because the pin is not usable", name, pin);
    return;
  }
  const bool up = readLevel(pin, INPUT_PULLUP);
  const bool down = readLevel(pin, INPUT_PULLDOWN);
  pinMode(pin, INPUT);
  say("%s (GPIO%d): with internal pull-up reads %s, with internal pull-down reads %s", name, pin,
      up ? "HIGH" : "LOW", down ? "HIGH" : "LOW");
  report(name, judgeLine(up, down));
}

void checkPinsAndLines() {
  say("--- I2C pins and electrical state ---");
  say("Configured: SDA=GPIO%d SCL=GPIO%d", MOTONAV_I2C_SDA, MOTONAV_I2C_SCL);
  report("SDA pin", judgeI2cPin(MOTONAV_I2C_SDA));
  report("SCL pin", judgeI2cPin(MOTONAV_I2C_SCL));
  report("pin pair", judgePinPair(MOTONAV_I2C_SDA, MOTONAV_I2C_SCL));
  checkLine("SDA line", MOTONAV_I2C_SDA);
  checkLine("SCL line", MOTONAV_I2C_SCL);
}

// --------------------------------- I2C bus ----------------------------------

// Scans addresses 1..126. Returns the number of devices found, or -1 if the I2C
// driver could not be started. `codes[e]` counts endTransmission() results.
int scan(int sda, int scl, uint32_t clockHz, uint8_t* found, int foundMax, int* codes) {
  for (int i = 0; i < 6; ++i) codes[i] = 0;
  if (!Wire.begin(sda, scl)) return -1;
  Wire.setClock(clockHz);
  Wire.setTimeOut(50);

  int count = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    const uint8_t error = Wire.endTransmission();
    if (error < 6) ++codes[error];
    if (error == 0 && count < foundMax) found[count++] = address;
  }
  Wire.end();
  return count;
}

// Probes one address repeatedly and returns how many probes were acknowledged.
int probe(int sda, int scl, uint8_t address, uint32_t clockHz, int attempts) {
  if (!Wire.begin(sda, scl)) return 0;
  Wire.setClock(clockHz);
  Wire.setTimeOut(50);
  int acknowledged = 0;
  for (int i = 0; i < attempts; ++i) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) ++acknowledged;
    delayMicroseconds(300);
  }
  Wire.end();
  return acknowledged;
}

void printAddresses(const char* label, const uint8_t* found, int count) {
  if (!verbose) return;
  if (count <= 0) {
    say("%s: no devices answered", label);
    return;
  }
  for (int i = 0; i < count; ++i) say("%s: device at 0x%02X", label, found[i]);
}

void checkBus() {
  say("--- I2C bus ---");
  uint8_t slow[kMaxFound];
  uint8_t fast[kMaxFound];
  int slowCodes[6];
  int fastCodes[6];

  const int slowCount = scan(MOTONAV_I2C_SDA, MOTONAV_I2C_SCL, kSlowClockHz, slow, kMaxFound, slowCodes);
  if (slowCount < 0) {
    report("I2C driver", Finding{Verdict::Fail, "the I2C driver could not be started on these pins"});
    return;
  }
  const int fastCount = scan(MOTONAV_I2C_SDA, MOTONAV_I2C_SCL, kFastClockHz, fast, kMaxFound, fastCodes);

  printAddresses("100 kHz", slow, slowCount);
  printAddresses("400 kHz", fast, fastCount < 0 ? 0 : fastCount);
  say("Scan results at 100 kHz: ack=%d nack-on-address=%d nack-on-data=%d other=%d timeout=%d",
      slowCodes[0], slowCodes[2], slowCodes[3], slowCodes[4], slowCodes[5]);
  say("(Informational. Empty addresses normally all report the same code. A mixture of codes can mean noise.)");

  for (int i = 0; i < slowCount; ++i) setBit(summary.foundMask, slow[i]);

  // Swapped-wires check, only if nothing answered.
  int swappedCount = 0;
  if (slowCount == 0) {
    uint8_t swapped[kMaxFound];
    int swappedCodes[6];
    swappedCount = scan(MOTONAV_I2C_SCL, MOTONAV_I2C_SDA, kSlowClockHz, swapped, kMaxFound, swappedCodes);
    if (swappedCount < 0) swappedCount = 0;
    printAddresses("swapped SDA/SCL", swapped, swappedCount);
  }

  const AddressFinding address = judgeOledAddress(slow, slowCount, MOTONAV_OLED_I2C_ADDR, swappedCount);
  report("OLED address", address.finding);
  summary.oledAddress = address.address;

  if (address.address < 0) return;

  say("OLED detected at 0x%02X", address.address);
  if (address.address != MOTONAV_OLED_I2C_ADDR) {
    say("ACTION: in firmware/platformio.ini set   -DMOTONAV_OLED_I2C_ADDR=0x%02X   then upload hw_check again",
        address.address);
  }

  const int ackSlow = probe(MOTONAV_I2C_SDA, MOTONAV_I2C_SCL, static_cast<uint8_t>(address.address),
                            kSlowClockHz, kProbeCount);
  const int ackFast = probe(MOTONAV_I2C_SDA, MOTONAV_I2C_SCL, static_cast<uint8_t>(address.address),
                            kFastClockHz, kProbeCount);
  say("Probes at 100 kHz: %d of %d acknowledged", ackSlow, kProbeCount);
  say("Probes at 400 kHz: %d of %d acknowledged", ackFast, kProbeCount);
  report("probes at 100 kHz", judgeProbes(ackSlow, kProbeCount));
  report("probes at 400 kHz", judgeProbes(ackFast, kProbeCount));
}

// -------------------------------- one run -----------------------------------

// `countAsNewRun` is false when a quiet run is repeated in full because it found a change.
void runOnce(bool beVerbose, bool countAsNewRun = true) {
  verbose = beVerbose;
  itemCount = 0;
  summary = Summary();

  if (countAsNewRun) ++runCount;
  say("%s", "");
  say("========== hw_check run %lu ==========", static_cast<unsigned long>(runCount));
  checkChipAndBoard();
  checkPinsAndLines();
  checkBus();

  say("--- Summary ---");
  for (int i = 0; i < itemCount; ++i) {
    if (items[i].finding.verdict != Verdict::Pass) {
      say("%s %s: %s", verdictLabel(items[i].finding.verdict), items[i].name, items[i].finding.text);
    }
  }
  if (verbose) {
    Serial.printf("HW_CHECK RESULT: %s  (%d pass, %d warn, %d fail)\n", verdictLabel(summary.overall),
                  summary.pass, summary.warn, summary.fail);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("MotoNav hw_check (diagnostic, not the MotoNav firmware)");
  Serial.printf("config: %dx%d display, expected address 0x%02X, SDA=GPIO%d SCL=GPIO%d\n",
                MOTONAV_OLED_WIDTH, MOTONAV_OLED_HEIGHT, static_cast<unsigned>(MOTONAV_OLED_I2C_ADDR),
                MOTONAV_I2C_SDA, MOTONAV_I2C_SCL);

  runOnce(true);
  last = summary;
  haveLast = true;
  lastRunMs = millis();
}

void loop() {
  if (millis() - lastRunMs < kRepeatMs) {
    delay(20);
    return;
  }
  lastRunMs = millis();

  // Quiet run first. Only print in full if something changed since the last run.
  runOnce(false);
  const bool changed = !haveLast || summary.overall != last.overall ||
                       summary.fail != last.fail || summary.warn != last.warn ||
                       !sameMask(summary.foundMask, last.foundMask);
  if (changed) {
    Serial.println();
    Serial.println("hw_check: something changed since the previous run. Full report:");
    runOnce(true, false);
    sameAsPreviousRuns = 0;
  } else {
    ++sameAsPreviousRuns;
    Serial.printf("run %lu: %s, OLED %s, same as the previous run (%lu in a row)\n",
                  static_cast<unsigned long>(runCount), verdictLabel(summary.overall),
                  summary.oledAddress >= 0 ? "found" : "NOT found",
                  static_cast<unsigned long>(sameAsPreviousRuns));
  }
  last = summary;
  haveLast = true;
}
