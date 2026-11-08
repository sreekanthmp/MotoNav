// Navigation data model. Pure data: no display, no BLE, no Arduino.
//
// This is the only thing the screens read. In Phase 2 the BLE receiver will
// fill the same structures, so no rendering code changes then.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace motonav {

// What the next maneuver is. REROUTING is deliberately not here: it is a
// navigation mode (see NavMode), as it is in Google's NavState.
//
// U-turns carry a side. The Google SDK distinguishes clockwise and
// counterclockwise U-turns, and in left-hand traffic they are mirrored, so a
// single "U-turn" icon would be wrong in one of the two cases.
enum class Maneuver : uint8_t {
  Straight,
  TurnLeft,
  TurnRight,
  SlightLeft,
  SlightRight,
  SharpLeft,
  SharpRight,
  UTurnLeft,
  UTurnRight,
  Roundabout,
  Arrived,
};
constexpr int kManeuverCount = 11;

enum class NavMode : uint8_t {
  Ready,       // connected, no navigation running (also "navigation stopped")
  Navigating,  // guidance active, next maneuver known
  Rerouting,   // guidance active, searching for a new route
  Arrived,     // destination reached
};

enum class Link : uint8_t { Disconnected, Connected };

constexpr size_t kRoadNameMax = 31;
constexpr uint32_t kDistanceUnknown = 0xFFFFFFFFu;

struct NavSnapshot {
  NavMode mode = NavMode::Ready;
  Maneuver maneuver = Maneuver::Straight;
  uint32_t distanceToManeuverM = kDistanceUnknown;
  uint8_t roundaboutExit = 0;  // 0 = unknown
  // RAM only. Road names must never be persisted (Google policy: persisting
  // them counts as scraping).
  char roadName[kRoadNameMax + 1] = {0};
};

// Everything the display needs to decide what to show.
struct DeviceState {
  Link link = Link::Disconnected;
  NavSnapshot nav;
};

// Copies `name` into the snapshot, truncating to kRoadNameMax bytes.
// nullptr clears the name.
void setRoadName(NavSnapshot& snapshot, const char* name);

}  // namespace motonav
