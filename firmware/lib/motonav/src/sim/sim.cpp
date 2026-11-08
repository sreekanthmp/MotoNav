#include "sim/sim.h"

namespace motonav {

namespace {

constexpr uint32_t kDisconnectedMs = 4000;
constexpr uint32_t kReadyMs = 4000;
constexpr uint32_t kManeuverMs = 6000;
constexpr uint32_t kReroutingMs = 4000;
constexpr uint32_t kArrivedMs = 5000;
constexpr uint32_t kStoppedMs = 3000;

constexpr uint32_t kStartDistanceM = 1500;
constexpr uint32_t kEndDistanceM = 20;

const Maneuver kOrder[kManeuverCount] = {
    Maneuver::Straight,   Maneuver::TurnLeft,   Maneuver::TurnRight,
    Maneuver::SlightLeft, Maneuver::SlightRight, Maneuver::SharpLeft,
    Maneuver::SharpRight, Maneuver::UTurnLeft,  Maneuver::UTurnRight,
    Maneuver::Roundabout, Maneuver::Arrived,
};

// Used by the running script. Includes a name that is too long for the display
// and one with no name.
const char* const kScriptRoads[] = {
    "Main Street",
    "MG Road",
    "Outer Ring Road",
    "National Highway 66",
    "Sir Mokshagundam Visvesvaraya Road",
    "",
    "Pacific Coast Highway",
};
constexpr int kScriptRoadCount = sizeof(kScriptRoads) / sizeof(kScriptRoads[0]);

// Used by the `d` command: every rule of the distance formatting, in order.
// Expected display: 450 m, 90 m, 0 m, 10 m, 1.0 km, 1.2 km, 9.9 km, 12 km,
// 999 km, --
const uint32_t kTestDistances[] = {450, 90, 0, 9, 975, 1200, 9949, 12000, 999500, kDistanceUnknown};
constexpr int kTestDistanceCount = sizeof(kTestDistances) / sizeof(kTestDistances[0]);

// Used by the `r` command. Expected display: Main Street / a name cut to
// "Sir Mokshagundam V..." / no road line / no road line (Malayalam only, no
// ASCII letters) / "Caf? Road".
const char* const kTestRoads[] = {
    "Main Street",
    "Sir Mokshagundam Visvesvaraya Road",
    "",
    "\xE0\xB4\xAE\xE0\xB4\xB2\xE0\xB4\xAA\xE0\xB5\x8D\xE0\xB4\xAA\xE0\xB5\x81\xE0\xB4\xB1\xE0\xB4\x82",
    "Caf\xC3\xA9 Road",
};
constexpr int kTestRoadCount = sizeof(kTestRoads) / sizeof(kTestRoads[0]);

constexpr uint32_t kLoopMs = kDisconnectedMs + kReadyMs +
                             kManeuverCount * kManeuverMs + kReroutingMs +
                             kArrivedMs + kStoppedMs;

DeviceState connected(NavMode mode) {
  DeviceState s;
  s.link = Link::Connected;
  s.nav.mode = mode;
  return s;
}

DeviceState navigating(int maneuverIndex, uint32_t distanceM, const char* road) {
  DeviceState s = connected(NavMode::Navigating);
  s.nav.maneuver = kOrder[maneuverIndex];
  s.nav.distanceToManeuverM = distanceM;
  s.nav.roundaboutExit = s.nav.maneuver == Maneuver::Roundabout ? 2 : 0;
  setRoadName(s.nav, road);
  return s;
}

}  // namespace

uint32_t SimScript::loopMs() { return kLoopMs; }

DeviceState SimScript::at(uint32_t ms) {
  uint32_t t = ms % kLoopMs;

  if (t < kDisconnectedMs) return DeviceState();  // link down
  t -= kDisconnectedMs;

  if (t < kReadyMs) return connected(NavMode::Ready);
  t -= kReadyMs;

  if (t < kManeuverCount * kManeuverMs) {
    const int index = static_cast<int>(t / kManeuverMs);
    const uint32_t within = t % kManeuverMs;
    const uint32_t distance =
        kStartDistanceM - ((kStartDistanceM - kEndDistanceM) * within) / kManeuverMs;
    return navigating(index, distance, kScriptRoads[index % kScriptRoadCount]);
  }
  t -= kManeuverCount * kManeuverMs;

  if (t < kReroutingMs) return connected(NavMode::Rerouting);
  t -= kReroutingMs;

  if (t < kArrivedMs) return connected(NavMode::Arrived);

  return connected(NavMode::Ready);  // navigation stopped
}

int SimController::testDistanceCount() { return kTestDistanceCount; }
uint32_t SimController::testDistance(int index) { return kTestDistances[index % kTestDistanceCount]; }
int SimController::testRoadCount() { return kTestRoadCount; }
const char* SimController::testRoad(int index) { return kTestRoads[index % kTestRoadCount]; }

void SimController::holdNavigating() {
  auto_ = false;
  held_ = navigating(heldManeuver_, kTestDistances[heldDistance_], kTestRoads[heldRoad_]);
}

bool SimController::handleCommand(char command, uint32_t nowMs) {
  switch (command) {
    case 'a':
      auto_ = true;
      autoStartMs_ = nowMs;
      return true;
    case '0':
      auto_ = false;
      held_ = DeviceState();
      return true;
    case '1':
      auto_ = false;
      held_ = connected(NavMode::Ready);
      return true;
    case '2':
      holdNavigating();
      return true;
    case '3':
      auto_ = false;
      held_ = connected(NavMode::Rerouting);
      return true;
    case '4':
      auto_ = false;
      held_ = connected(NavMode::Arrived);
      return true;
    case 'n':
      heldManeuver_ = (heldManeuver_ + 1) % kManeuverCount;
      holdNavigating();
      return true;
    case 'd':
      heldDistance_ = (heldDistance_ + 1) % kTestDistanceCount;
      holdNavigating();
      return true;
    case 'r':
      heldRoad_ = (heldRoad_ + 1) % kTestRoadCount;
      holdNavigating();
      return true;
    default:
      return false;
  }
}

DeviceState SimController::state(uint32_t nowMs) const {
  return auto_ ? SimScript::at(nowMs - autoStartMs_) : held_;
}

}  // namespace motonav
