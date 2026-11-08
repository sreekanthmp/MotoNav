// ScreenView: exactly what is visible on the display, already formatted.
//
// Building the view is where navigation data is turned into display
// decisions (which screen, rounded distance text, cleaned-up road name).
// Two states that look identical produce equal views, so the Presenter can
// skip redrawing when nothing visible changed.
#pragma once

#include "core/distance_format.h"
#include "core/nav_model.h"

namespace motonav {

enum class Screen : uint8_t {
  Disconnected,
  Ready,
  Navigating,
  Rerouting,
  Arrived,
};
constexpr int kScreenCount = 5;

struct ScreenView {
  Screen screen = Screen::Disconnected;
  // The fields below are only meaningful for Screen::Navigating and are left
  // zeroed otherwise, so stale values cannot cause a needless redraw.
  Maneuver maneuver = Maneuver::Straight;
  uint8_t roundaboutExit = 0;
  DistanceText distance;
  char road[kRoadNameMax + 1] = {0};
};

bool operator==(const ScreenView& a, const ScreenView& b);
inline bool operator!=(const ScreenView& a, const ScreenView& b) {
  return !(a == b);
}

ScreenView buildView(const DeviceState& state);

const char* screenName(Screen s);  // "Navigating", for logs and serial output

// Writes a one-line, human-readable description of exactly what the view shows,
// for example:  screen=Navigating icon=TurnLeft distance="450 m" road="Main Street"
// Used by the firmware's serial output so a tester can compare screen and text.
void describeView(const ScreenView& view, char* out, size_t outSize);

// Short upper-case instruction for a maneuver ("TURN LEFT"). Every string is
// short enough for the navigation panel on a 128-pixel-wide display.
const char* instructionText(Maneuver m);

}  // namespace motonav
