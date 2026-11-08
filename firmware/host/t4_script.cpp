// Prints the exact "expect:" line the firmware prints after each serial command of
// the T4 walkthrough, by running the real SimController and describeView. HOST ONLY.
#include <stdio.h>

#include <initializer_list>

#include "sim/sim.h"
#include "ui/view.h"

using namespace motonav;

static void press(SimController& c, char key) {
  char line[200];
  if (!c.handleCommand(key, 0)) {
    printf("> %c   (not a command)\n", key);
    return;
  }
  if (c.isAuto()) {
    printf("> %c   (automatic mode, no expect line)\n", key);
    return;
  }
  describeView(buildView(c.state(0)), line, sizeof(line));
  printf("> %c   expect: %s\n", key, line);
}

int main() {
  SimController c;
  puts("# Screens");
  for (char k : {'0', '1', '2', '3', '4'}) press(c, k);
  puts("# Maneuvers: press 2, then n eleven times (the eleventh wraps back to Straight)");
  press(c, '2');
  for (int i = 0; i < kManeuverCount; ++i) press(c, 'n');
  puts("# Distances: press d ten times (the tenth wraps back to 450 m)");
  for (int i = 0; i < SimController::testDistanceCount(); ++i) press(c, 'd');
  puts("# Road names: press r five times (the fifth wraps back to Main Street)");
  for (int i = 0; i < SimController::testRoadCount(); ++i) press(c, 'r');
  puts("# Back to automatic");
  press(c, 'a');
  return 0;
}
