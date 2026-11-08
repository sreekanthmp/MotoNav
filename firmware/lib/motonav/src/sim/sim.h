// Simulated navigation data for Phase 1. Replaced by BLE input in Phase 2;
// nothing here depends on a display or on hardware.
#pragma once

#include <stdint.h>

#include "core/nav_model.h"

namespace motonav {

// A fixed script that repeats forever:
//   disconnected -> ready -> every maneuver (distance counting down)
//   -> rerouting -> arrived -> ready ("navigation stopped") -> repeat
class SimScript {
 public:
  static uint32_t loopMs();
  // `ms` is time since the script started; it wraps at loopMs().
  static DeviceState at(uint32_t ms);
};

// Serial commands for inspecting one screen on the device:
//   a   run the script automatically (restarts it)
//   0   hold the Disconnected screen      1   hold Ready
//   2   hold Navigating                   3   hold Rerouting
//   4   hold Arrived
//   n   hold Navigating and step to the next maneuver (wraps after 11)
//   d   hold Navigating and step to the next test distance
//   r   hold Navigating and step to the next test road name
// Held Navigating shows the current maneuver, distance and road together, so
// the three step commands can be combined.
class SimController {
 public:
  // Returns false for characters that are not commands.
  bool handleCommand(char command, uint32_t nowMs);
  DeviceState state(uint32_t nowMs) const;
  bool isAuto() const { return auto_; }

  // The test values the `d` and `r` commands cycle through. Exposed so tests
  // and documentation cannot drift from the firmware.
  static int testDistanceCount();
  static uint32_t testDistance(int index);
  static int testRoadCount();
  static const char* testRoad(int index);

 private:
  void holdNavigating();

  bool auto_ = true;
  uint32_t autoStartMs_ = 0;
  DeviceState held_;
  int heldManeuver_ = 0;
  int heldDistance_ = 0;
  int heldRoad_ = 0;
};

}  // namespace motonav
