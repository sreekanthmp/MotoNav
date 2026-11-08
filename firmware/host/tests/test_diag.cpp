// Verdict logic for the hardware diagnostics, the frame-consistency check, and
// the serial-output helpers used during physical testing.
#include <string.h>

#include <set>
#include <string>

#include "core/distance_format.h"
#include "diag/frame_check.h"
#include "diag/hw_verdict.h"
#include "sim/sim.h"
#include "test_framework.h"
#include "ui/view.h"

using namespace motonav;

namespace {
bool contains(const char* text, const char* part) { return strstr(text, part) != nullptr; }
}  // namespace

// ------------------------------ chip and board -----------------------------

TEST(diag_chip_must_be_classic_esp32) {
  CHECK(judgeChip(true).verdict == Verdict::Pass);
  CHECK(judgeChip(false).verdict == Verdict::Fail);
  CHECK(contains(judgeChip(false).text, "Stop"));
}

TEST(diag_ble_is_required_wifi_is_not) {
  CHECK(judgeRadios(true, true).verdict == Verdict::Pass);
  CHECK(judgeRadios(false, true).verdict == Verdict::Pass);
  CHECK(judgeRadios(true, false).verdict == Verdict::Fail);
  CHECK(judgeRadios(false, false).verdict == Verdict::Fail);
}

TEST(diag_cores_and_clock) {
  CHECK(judgeCores(2, 240).verdict == Verdict::Pass);
  CHECK(judgeCores(1, 240).verdict == Verdict::Warn);
  CHECK(judgeCores(2, 160).verdict == Verdict::Warn);
}

TEST(diag_flash_size_rules) {
  const uint32_t mb = 1024u * 1024u;
  // Physical size read from the chip and agreeing with the SDK: the only clean pass.
  CHECK(judgeFlashSize(4 * mb, 4 * mb).verdict == Verdict::Pass);
  // Too small is a hard failure whichever source says so.
  CHECK(judgeFlashSize(2 * mb, 4 * mb).verdict == Verdict::Fail);
  CHECK(judgeFlashSize(0, 2 * mb).verdict == Verdict::Fail);
  CHECK(judgeFlashSize(0, 0).verdict == Verdict::Fail);
  // SDK and chip disagree (both at least 4 MB): warn.
  CHECK(judgeFlashSize(8 * mb, 4 * mb).verdict == Verdict::Warn);
  // Larger than expected: warn, not fail.
  CHECK(judgeFlashSize(8 * mb, 8 * mb).verdict == Verdict::Warn);
  CHECK(judgeFlashSize(16 * mb, 16 * mb).verdict == Verdict::Warn);
  // Physical size unreadable: never a clean pass.
  CHECK(judgeFlashSize(0, 4 * mb).verdict == Verdict::Warn);
  // A chip that is physically small but claims 4 MB in the image header must fail.
  CHECK(judgeFlashSize(2 * mb, 4 * mb).verdict == Verdict::Fail);
}

TEST(diag_jedec_capacity_codes) {
  CHECK_INT(flashBytesFromJedecCapacity(0x16), 4 * 1024 * 1024);   // 32 Mbit, the usual 4 MB part
  CHECK_INT(flashBytesFromJedecCapacity(0x15), 2 * 1024 * 1024);
  CHECK_INT(flashBytesFromJedecCapacity(0x17), 8 * 1024 * 1024);
  CHECK_INT(flashBytesFromJedecCapacity(0x18), 16 * 1024 * 1024);
  CHECK_INT(flashBytesFromJedecCapacity(0x00), 0);                 // implausible codes
  CHECK_INT(flashBytesFromJedecCapacity(0x0F), 0);
  CHECK_INT(flashBytesFromJedecCapacity(0x1A), 0);
  CHECK_INT(flashBytesFromJedecCapacity(0xFF), 0);
}

// ------------------------------- I2C pins ----------------------------------

TEST(diag_i2c_pin_rules) {
  CHECK(judgeI2cPin(21).verdict == Verdict::Pass);
  CHECK(judgeI2cPin(22).verdict == Verdict::Pass);
  CHECK(judgeI2cPin(18).verdict == Verdict::Pass);
  // Flash pins.
  for (int gpio = 6; gpio <= 11; ++gpio) CHECK(judgeI2cPin(gpio).verdict == Verdict::Fail);
  // Input-only pins.
  for (int gpio = 34; gpio <= 39; ++gpio) CHECK(judgeI2cPin(gpio).verdict == Verdict::Fail);
  // Strapping pins: usable but risky.
  const int strapping[] = {0, 2, 5, 12, 15};
  for (int gpio : strapping) CHECK(judgeI2cPin(gpio).verdict == Verdict::Warn);
  // GPIOs that do not exist on the classic ESP32.
  const int missing[] = {-1, 20, 24, 28, 29, 30, 31, 40, 100};
  for (int gpio : missing) CHECK(judgeI2cPin(gpio).verdict == Verdict::Fail);
}

TEST(diag_pin_pair) {
  CHECK(judgePinPair(21, 22).verdict == Verdict::Pass);
  CHECK(judgePinPair(21, 21).verdict == Verdict::Fail);
}

TEST(diag_line_states) {
  // High with the pull-up and still high against the pull-down: external pull-up present.
  CHECK(judgeLine(true, true).verdict == Verdict::Pass);
  // High with the pull-up but low against the pull-down: no external pull-up.
  CHECK(judgeLine(true, false).verdict == Verdict::Warn);
  CHECK(contains(judgeLine(true, false).text, "internal pull-ups"));
  // Low even with the pull-up: short to ground.
  CHECK(judgeLine(false, false).verdict == Verdict::Fail);
  CHECK(contains(judgeLine(false, false).text, "shorted"));
  // Contradictory: something is driving the line.
  CHECK(judgeLine(false, true).verdict == Verdict::Fail);
}

// -------------------------------- I2C bus ----------------------------------

TEST(diag_probe_reliability) {
  CHECK(judgeProbes(200, 200).verdict == Verdict::Pass);
  CHECK(judgeProbes(199, 200).verdict == Verdict::Fail);  // a single miss is not a pass
  CHECK(judgeProbes(1, 200).verdict == Verdict::Fail);
  CHECK(judgeProbes(0, 200).verdict == Verdict::Fail);
  CHECK(judgeProbes(0, 0).verdict == Verdict::Warn);
  CHECK(contains(judgeProbes(150, 200).text, "intermittent"));
}

TEST(diag_oled_address_decisions) {
  const uint8_t only3C[] = {0x3C};
  const uint8_t only3D[] = {0x3D};
  const uint8_t both[] = {0x3C, 0x3D};
  const uint8_t other[] = {0x50};
  const uint8_t oledAndOther[] = {0x3C, 0x68};

  // At the configured address: pass, and the address is returned.
  AddressFinding f = judgeOledAddress(only3C, 1, 0x3C, 0);
  CHECK(f.finding.verdict == Verdict::Pass);
  CHECK_INT(f.address, 0x3C);

  // At the other typical address: warn, and the address to use is returned.
  f = judgeOledAddress(only3D, 1, 0x3C, 0);
  CHECK(f.finding.verdict == Verdict::Warn);
  CHECK_INT(f.address, 0x3D);
  CHECK(contains(f.finding.text, "platformio.ini"));

  f = judgeOledAddress(only3C, 1, 0x3D, 0);
  CHECK(f.finding.verdict == Verdict::Warn);
  CHECK_INT(f.address, 0x3C);

  // Both addresses answering is suspicious.
  f = judgeOledAddress(both, 2, 0x3C, 0);
  CHECK(f.finding.verdict == Verdict::Warn);
  CHECK_INT(f.address, 0x3C);
  f = judgeOledAddress(both, 2, 0x50, 0);
  CHECK_INT(f.address, -1);

  // Another device on the bus does not hide the OLED.
  f = judgeOledAddress(oledAndOther, 2, 0x3C, 0);
  CHECK(f.finding.verdict == Verdict::Pass);
  CHECK_INT(f.address, 0x3C);

  // A device that is not at a typical OLED address is never assumed to be the OLED.
  f = judgeOledAddress(other, 1, 0x3C, 0);
  CHECK(f.finding.verdict == Verdict::Warn);
  CHECK_INT(f.address, -1);

  // Nothing found: fail; swapped wires are called out.
  f = judgeOledAddress(nullptr, 0, 0x3C, 0);
  CHECK(f.finding.verdict == Verdict::Fail);
  CHECK_INT(f.address, -1);
  CHECK(!contains(f.finding.text, "swapped"));
  f = judgeOledAddress(nullptr, 0, 0x3C, 1);
  CHECK(f.finding.verdict == Verdict::Fail);
  CHECK(contains(f.finding.text, "swapped"));
}

TEST(diag_verdict_ordering) {
  CHECK(worseOf(Verdict::Pass, Verdict::Warn) == Verdict::Warn);
  CHECK(worseOf(Verdict::Fail, Verdict::Warn) == Verdict::Fail);
  CHECK(worseOf(Verdict::Pass, Verdict::Pass) == Verdict::Pass);
  CHECK_STR(verdictLabel(Verdict::Pass), "PASS");
  CHECK_STR(verdictLabel(Verdict::Warn), "WARN");
  CHECK_STR(verdictLabel(Verdict::Fail), "FAIL");
}

TEST(diag_every_finding_has_text) {
  // No finding may be silent: each one must tell the person at the bench something.
  const Finding all[] = {
      judgeChip(true), judgeChip(false), judgeRadios(true, true), judgeRadios(false, false),
      judgeCores(2, 240), judgeCores(1, 80), judgeFlashSize(0, 0), judgeFlashSize(4 << 20, 4 << 20),
      judgeI2cPin(21), judgeI2cPin(0), judgeI2cPin(7), judgeI2cPin(35), judgeI2cPin(99),
      judgePinPair(1, 1), judgeLine(true, true), judgeLine(true, false), judgeLine(false, false),
      judgeLine(false, true), judgeProbes(0, 0), judgeProbes(5, 5), judgeProbes(1, 5),
  };
  for (const Finding& f : all) CHECK(f.text != nullptr && strlen(f.text) > 10);
}

// --------------------------- frame consistency -----------------------------

TEST(frame_hash_known_vectors) {
  // Published FNV-1a 32-bit test vectors.
  CHECK_INT(fnv1a32(nullptr, 0), 0x811C9DC5u);
  const uint8_t a[] = {'a'};
  CHECK_INT(fnv1a32(a, 1), 0xE40C292Cu);
  const uint8_t foobar[] = {'f', 'o', 'o', 'b', 'a', 'r'};
  CHECK_INT(fnv1a32(foobar, 6), 0xBF9CF968u);
  // Chaining equals one pass over the concatenation.
  const uint32_t chained = fnv1a32(foobar + 3, 3, fnv1a32(foobar, 3));
  CHECK_INT(chained, 0xBF9CF968u);
}

TEST(frame_consistency_detects_change) {
  FrameConsistency c;
  CHECK(c.check(1, 0xAAAA) == FrameCheck::First);
  CHECK(c.check(1, 0xAAAA) == FrameCheck::Match);
  CHECK(c.check(2, 0xBBBB) == FrameCheck::First);   // keys are independent
  CHECK(c.check(2, 0xBBBB) == FrameCheck::Match);
  CHECK_INT(c.mismatches(), 0);

  CHECK(c.check(1, 0xAAAB) == FrameCheck::Mismatch);
  CHECK_INT(c.mismatches(), 1);
  CHECK(c.check(1, 0xAAAA) == FrameCheck::Match);   // the reference is not overwritten
  CHECK(c.check(1, 0xCCCC) == FrameCheck::Mismatch);
  CHECK_INT(c.mismatches(), 2);

  CHECK(c.check(-1, 5) == FrameCheck::Match);       // out-of-range keys are ignored
  CHECK(c.check(99, 5) == FrameCheck::Match);
  CHECK_INT(c.mismatches(), 2);
}

// ----------------------- serial helpers for physical tests ------------------

TEST(describe_view_text) {
  char out[160];

  describeView(buildView(DeviceState()), out, sizeof(out));
  CHECK_STR(out, "screen=Disconnected");

  DeviceState s;
  s.link = Link::Connected;
  describeView(buildView(s), out, sizeof(out));
  CHECK_STR(out, "screen=Ready");

  s.nav.mode = NavMode::Navigating;
  s.nav.maneuver = Maneuver::TurnLeft;
  s.nav.distanceToManeuverM = 450;
  setRoadName(s.nav, "Main Street");
  describeView(buildView(s), out, sizeof(out));
  CHECK_STR(out, "screen=Navigating icon=TurnLeft distance=\"450 m\" road=\"Main Street\"");

  s.nav.maneuver = Maneuver::Roundabout;
  s.nav.roundaboutExit = 2;
  s.nav.distanceToManeuverM = 1200;
  describeView(buildView(s), out, sizeof(out));
  CHECK_STR(out, "screen=Navigating icon=Roundabout exit=2 distance=\"1.2 km\" road=\"Main Street\"");

  s.nav.distanceToManeuverM = kDistanceUnknown;
  describeView(buildView(s), out, sizeof(out));
  CHECK(contains(out, "distance=\"--\""));

  char tiny[8];
  describeView(buildView(s), tiny, sizeof(tiny));  // must truncate safely
  CHECK(strlen(tiny) < sizeof(tiny));
  describeView(buildView(s), out, 0);              // must not write
}

TEST(names_cover_every_enum_value) {
  std::set<std::string> maneuvers;
  for (int i = 0; i < kManeuverCount; ++i) {
    const char* n = maneuverName(static_cast<Maneuver>(i));
    CHECK(strcmp(n, "?") != 0);
    maneuvers.insert(n);
  }
  CHECK_INT(maneuvers.size(), kManeuverCount);  // all distinct

  std::set<std::string> screens;
  for (int i = 0; i < kScreenCount; ++i) {
    const char* n = screenName(static_cast<Screen>(i));
    CHECK(strcmp(n, "?") != 0);
    screens.insert(n);
  }
  CHECK_INT(screens.size(), kScreenCount);
}

// The exact strings the documentation promises for the `d` command.
TEST(sim_test_distances_display_as_documented) {
  const char* expected[] = {"450 m", "90 m", "0 m", "10 m", "1.0 km",
                            "1.2 km", "9.9 km", "12 km", "999 km", "--"};
  CHECK_INT(SimController::testDistanceCount(), 10);
  for (int i = 0; i < 10; ++i) {
    const DistanceText d = formatDistance(SimController::testDistance(i));
    char text[16];
    snprintf(text, sizeof(text), "%s%s%s", d.value, d.unit[0] ? " " : "", d.unit);
    if (strcmp(text, expected[i]) != 0) printf("    distance %d shows \"%s\", expected \"%s\"\n", i, text, expected[i]);
    CHECK_STR(text, expected[i]);
  }
}

// The exact road lines the documentation promises for the `r` command.
TEST(sim_test_roads_display_as_documented) {
  const char* expected[] = {"Main Street", "Sir Mokshagundam Visvesvaraya R", "", "", "Caf? Road"};
  CHECK_INT(SimController::testRoadCount(), 5);
  for (int i = 0; i < 5; ++i) {
    DeviceState s;
    s.link = Link::Connected;
    s.nav.mode = NavMode::Navigating;
    setRoadName(s.nav, SimController::testRoad(i));
    const ScreenView view = buildView(s);  // keep it alive: `road` points into it
    CHECK_STR(view.road, expected[i]);
  }
}

TEST(sim_d_and_r_commands_step_and_wrap) {
  SimController c;
  CHECK(c.handleCommand('2', 0));
  CHECK_INT(buildView(c.state(0)).distance.value[0] == '4', 1);  // starts at 450 m

  std::set<std::string> shown;
  for (int i = 0; i < SimController::testDistanceCount(); ++i) {
    CHECK(c.handleCommand('d', 0));
    CHECK(!c.isAuto());
    CHECK(buildView(c.state(0)).screen == Screen::Navigating);
    const DistanceText d = buildView(c.state(0)).distance;
    shown.insert(std::string(d.value) + d.unit);
  }
  CHECK_INT(shown.size(), SimController::testDistanceCount());  // all different, then wrapped to 450

  // `d` and `r` keep the maneuver chosen with `n`.
  CHECK(c.handleCommand('n', 0));
  const Maneuver m = buildView(c.state(0)).maneuver;
  CHECK(c.handleCommand('r', 0));
  CHECK(buildView(c.state(0)).maneuver == m);
  CHECK(c.handleCommand('d', 0));
  CHECK(buildView(c.state(0)).maneuver == m);

  // `r` cycles through the road names and wraps.
  SimController r;
  std::set<std::string> roads;
  for (int i = 0; i < SimController::testRoadCount(); ++i) {
    CHECK(r.handleCommand('r', 0));
    roads.insert(buildView(r.state(0)).road);
  }
  CHECK_INT(roads.size(), 4);  // five inputs, but two of them display as "no road"
}
