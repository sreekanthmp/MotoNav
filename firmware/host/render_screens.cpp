// Renders every screen to PBM files so they can be reviewed without hardware.
//   ./build/render_screens out
#include <stdio.h>
#include <sys/stat.h>

#include <string>

#include "bitmap_canvas.h"
#include "core/nav_model.h"
#include "ui/screens.h"
#include "ui/view.h"

using namespace motonav;

static const char* maneuverName(Maneuver m) {
  static const char* const names[kManeuverCount] = {
      "straight",    "turn_left",  "turn_right",  "slight_left", "slight_right", "sharp_left",
      "sharp_right", "uturn_left", "uturn_right", "roundabout",  "arrived"};
  return names[static_cast<int>(m)];
}

static void dump(const std::string& dir, const std::string& name, int w, int h,
                 const DeviceState& state) {
  hosttest::BitmapCanvas canvas(w, h);
  renderScreen(canvas, buildView(state));
  char file[256];
  snprintf(file, sizeof(file), "%s/%dx%d_%s.pbm", dir.c_str(), w, h, name.c_str());
  if (!canvas.writePbm(file)) fprintf(stderr, "cannot write %s\n", file);
}

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : "out";
  mkdir(dir.c_str(), 0755);

  const int sizes[][2] = {{128, 64}, {128, 32}};
  for (const auto& size : sizes) {
    const int w = size[0], h = size[1];

    dump(dir, "disconnected", w, h, DeviceState());

    DeviceState ready;
    ready.link = Link::Connected;
    dump(dir, "ready", w, h, ready);

    for (int i = 0; i < kManeuverCount; ++i) {
      DeviceState s = ready;
      s.nav.mode = NavMode::Navigating;
      s.nav.maneuver = static_cast<Maneuver>(i);
      s.nav.distanceToManeuverM = 450;
      s.nav.roundaboutExit = 2;
      setRoadName(s.nav, "Sir Mokshagundam Visvesvaraya Road");
      dump(dir, std::string("nav_") + maneuverName(s.nav.maneuver), w, h, s);
    }

    // A few distance formats on one maneuver.
    const uint32_t distances[] = {0, 90, 1200, 12000, kDistanceUnknown};
    for (uint32_t d : distances) {
      DeviceState s = ready;
      s.nav.mode = NavMode::Navigating;
      s.nav.maneuver = Maneuver::TurnRight;
      s.nav.distanceToManeuverM = d;
      setRoadName(s.nav, "MG Road");
      dump(dir, "dist_" + std::to_string(d == kDistanceUnknown ? 0 : d) + (d == kDistanceUnknown ? "_unknown" : ""), w, h, s);
    }

    DeviceState rerouting = ready;
    rerouting.nav.mode = NavMode::Rerouting;
    dump(dir, "rerouting", w, h, rerouting);

    DeviceState arrived = ready;
    arrived.nav.mode = NavMode::Arrived;
    dump(dir, "arrived", w, h, arrived);
  }
  printf("wrote screens to %s/\n", dir.c_str());
  return 0;
}
