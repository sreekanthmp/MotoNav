// Redraw policy (no flicker) and the simulated navigation source.
#include <set>

#include "bitmap_canvas.h"
#include "sim/sim.h"
#include "test_framework.h"
#include "ui/presenter.h"
#include "ui/screens.h"

using namespace motonav;
using hosttest::BitmapCanvas;

namespace {

DeviceState navigating(uint32_t distance, const char* road = "MG Road") {
  DeviceState s;
  s.link = Link::Connected;
  s.nav.mode = NavMode::Navigating;
  s.nav.maneuver = Maneuver::TurnLeft;
  s.nav.distanceToManeuverM = distance;
  setRoadName(s.nav, road);
  return s;
}

bool sameState(const DeviceState& a, const DeviceState& b) {
  return a.link == b.link && a.nav.mode == b.nav.mode && a.nav.maneuver == b.nav.maneuver &&
         a.nav.distanceToManeuverM == b.nav.distanceToManeuverM &&
         a.nav.roundaboutExit == b.nav.roundaboutExit &&
         strcmp(a.nav.roadName, b.nav.roadName) == 0;
}

}  // namespace

// ------------------------------- Presenter ---------------------------------

TEST(presenter_draws_first_frame_then_only_on_visible_change) {
  BitmapCanvas canvas(128, 64);
  Presenter p(canvas);

  CHECK(p.update(navigating(452)));
  CHECK_INT(canvas.presentCount(), 1);

  CHECK(!p.update(navigating(452)));  // identical
  CHECK(!p.update(navigating(448)));  // both display "450 m"
  CHECK_INT(canvas.presentCount(), 1);

  CHECK(p.update(navigating(520)));   // "500 m"
  CHECK(p.update(navigating(520, "Other Road")));
  CHECK_INT(canvas.presentCount(), 3);
}

TEST(presenter_ignores_navigation_data_while_disconnected) {
  BitmapCanvas canvas(128, 64);
  Presenter p(canvas);
  CHECK(p.update(navigating(450)));

  DeviceState down = navigating(450);
  down.link = Link::Disconnected;
  CHECK(p.update(down));  // link loss is visible
  down.nav.distanceToManeuverM = 100;
  down.nav.maneuver = Maneuver::Roundabout;
  CHECK(!p.update(down));  // stale data changing is not
  CHECK_INT(canvas.presentCount(), 2);
}

TEST(presenter_invalidate_forces_a_redraw) {
  BitmapCanvas canvas(128, 64);
  Presenter p(canvas);
  CHECK(p.update(DeviceState()));
  CHECK(!p.update(DeviceState()));
  p.invalidate();  // e.g. the display was re-initialised
  CHECK(p.update(DeviceState()));
  CHECK_INT(canvas.presentCount(), 2);
}

// Replays two full loops of the script and checks, independently of the
// presenter, how many visible changes there were.
TEST(presenter_flushes_only_when_the_view_changes_over_a_full_script) {
  BitmapCanvas canvas(128, 64);
  Presenter p(canvas);

  const uint32_t stepMs = 100;
  const uint32_t total = 2 * SimScript::loopMs();
  int expectedChanges = 0;
  int ticks = 0;
  ScreenView previous;
  bool havePrevious = false;

  for (uint32_t t = 0; t < total; t += stepMs) {
    const DeviceState s = SimScript::at(t);
    const ScreenView v = buildView(s);
    if (!havePrevious || v != previous) ++expectedChanges;
    previous = v;
    havePrevious = true;
    ++ticks;

    const int before = canvas.presentCount();
    p.update(s);

    if (canvas.presentCount() != before) {
      // Whenever a frame was sent, it must equal a fresh render: no leftovers.
      BitmapCanvas fresh(128, 64);
      renderScreen(fresh, v);
      CHECK(canvas.hash() == fresh.hash());
    }
  }

  CHECK_INT(canvas.presentCount(), expectedChanges);
  CHECK_INT(canvas.outOfBounds(), 0);
  printf("    (%d ticks, %d frames presented)\n", ticks, canvas.presentCount());
  CHECK(canvas.presentCount() < ticks / 2);  // far fewer flushes than ticks
}

// --------------------------------- Sim -------------------------------------

TEST(sim_loop_length_and_wrap) {
  CHECK_INT(SimScript::loopMs(), 86000);
  CHECK(sameState(SimScript::at(1234), SimScript::at(1234 + SimScript::loopMs())));
  CHECK(sameState(SimScript::at(55555), SimScript::at(55555)));
}

TEST(sim_covers_every_screen_and_maneuver) {
  std::set<int> screens, maneuvers;
  for (uint32_t t = 0; t < SimScript::loopMs(); t += 100) {
    const DeviceState s = SimScript::at(t);
    const ScreenView v = buildView(s);
    screens.insert(static_cast<int>(v.screen));
    if (v.screen == Screen::Navigating) maneuvers.insert(static_cast<int>(v.maneuver));
  }
  CHECK_INT(screens.size(), kScreenCount);
  CHECK_INT(maneuvers.size(), kManeuverCount);
}

TEST(sim_script_order) {
  CHECK(buildView(SimScript::at(0)).screen == Screen::Disconnected);
  CHECK(buildView(SimScript::at(4000)).screen == Screen::Ready);
  CHECK(buildView(SimScript::at(8000)).screen == Screen::Navigating);
  CHECK(buildView(SimScript::at(8000 + 11 * 6000)).screen == Screen::Rerouting);
  CHECK(buildView(SimScript::at(8000 + 11 * 6000 + 4000)).screen == Screen::Arrived);
  CHECK(buildView(SimScript::at(SimScript::loopMs() - 1)).screen == Screen::Ready);  // stopped
}

TEST(sim_distance_counts_down_within_each_maneuver) {
  for (int m = 0; m < kManeuverCount; ++m) {
    const uint32_t start = 8000 + static_cast<uint32_t>(m) * 6000;
    uint32_t previous = 0xFFFFFFFFu;
    for (uint32_t t = start; t < start + 6000; t += 50) {
      const DeviceState s = SimScript::at(t);
      CHECK(s.nav.mode == NavMode::Navigating);
      CHECK_INT(static_cast<int>(s.nav.maneuver), m);
      CHECK(s.nav.distanceToManeuverM <= previous);
      CHECK(s.nav.distanceToManeuverM >= 20 && s.nav.distanceToManeuverM <= 1500);
      CHECK(strlen(s.nav.roadName) <= kRoadNameMax);
      CHECK((s.nav.maneuver == Maneuver::Roundabout) == (s.nav.roundaboutExit == 2));
      previous = s.nav.distanceToManeuverM;
    }
  }
}

TEST(sim_controller_commands) {
  SimController c;
  CHECK(c.isAuto());
  CHECK(sameState(c.state(5000), SimScript::at(5000)));

  CHECK(!c.handleCommand('x', 0));
  CHECK(c.isAuto());

  CHECK(c.handleCommand('4', 100));
  CHECK(!c.isAuto());
  CHECK(buildView(c.state(999999)).screen == Screen::Arrived);

  CHECK(c.handleCommand('0', 0));
  CHECK(buildView(c.state(0)).screen == Screen::Disconnected);
  CHECK(c.handleCommand('1', 0));
  CHECK(buildView(c.state(0)).screen == Screen::Ready);
  CHECK(c.handleCommand('3', 0));
  CHECK(buildView(c.state(0)).screen == Screen::Rerouting);

  // 'n' steps through all maneuvers and wraps.
  CHECK(c.handleCommand('2', 0));
  Maneuver first = buildView(c.state(0)).maneuver;
  std::set<int> seen;
  seen.insert(static_cast<int>(first));
  for (int i = 0; i < kManeuverCount; ++i) {
    CHECK(c.handleCommand('n', 0));
    seen.insert(static_cast<int>(buildView(c.state(0)).maneuver));
    CHECK(buildView(c.state(0)).screen == Screen::Navigating);
  }
  CHECK_INT(seen.size(), kManeuverCount);

  // 'a' restarts the script from the moment it is pressed.
  CHECK(c.handleCommand('a', 70000));
  CHECK(c.isAuto());
  CHECK(sameState(c.state(70000), SimScript::at(0)));
  CHECK(sameState(c.state(75000), SimScript::at(5000)));
}
