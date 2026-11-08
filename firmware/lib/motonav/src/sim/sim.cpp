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

// Includes a name that is too long for the display and one with no name.
const char* const kRoads[] = {
    "Main Street",
    "MG Road",
    "Outer Ring Road",
    "National Highway 66",
    "Sir Mokshagundam Visvesvaraya Road",
    "",
    "Pacific Coast Highway",
};
constexpr int kRoadCount = sizeof(kRoads) / sizeof(kRoads[0]);

constexpr uint32_t kLoopMs = kDisconnectedMs + kReadyMs +
                             kManeuverCount * kManeuverMs + kReroutingMs +
                             kArrivedMs + kStoppedMs;

DeviceState connected(NavMode mode) {
  DeviceState s;
  s.link = Link::Connected;
  s.nav.mode = mode;
  return s;
}

DeviceState navigating(int maneuverIndex, uint32_t distanceM) {
  DeviceState s = connected(NavMode::Navigating);
  s.nav.maneuver = kOrder[maneuverIndex];
  s.nav.distanceToManeuverM = distanceM;
  s.nav.roundaboutExit = s.nav.maneuver == Maneuver::Roundabout ? 2 : 0;
  setRoadName(s.nav, kRoads[maneuverIndex % kRoadCount]);
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
    return navigating(index, distance);
  }
  t -= kManeuverCount * kManeuverMs;

  if (t < kReroutingMs) return connected(NavMode::Rerouting);
  t -= kReroutingMs;

  if (t < kArrivedMs) return connected(NavMode::Arrived);

  return connected(NavMode::Ready);  // navigation stopped
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
      auto_ = false;
      held_ = navigating(heldManeuver_, 450);
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
      auto_ = false;
      heldManeuver_ = (heldManeuver_ + 1) % kManeuverCount;
      held_ = navigating(heldManeuver_, 450);
      return true;
    default:
      return false;
  }
}

DeviceState SimController::state(uint32_t nowMs) const {
  return auto_ ? SimScript::at(nowMs - autoStartMs_) : held_;
}

}  // namespace motonav
