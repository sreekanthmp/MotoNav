// Text helpers and ScreenView building.
#include <string.h>

#include "test_framework.h"
#include "ui/canvas.h"
#include "ui/screens.h"
#include "ui/text.h"
#include "ui/view.h"

using namespace motonav;

namespace {

DeviceState connected(NavMode mode) {
  DeviceState s;
  s.link = Link::Connected;
  s.nav.mode = mode;
  return s;
}

DeviceState navigating(Maneuver m, uint32_t distance, const char* road) {
  DeviceState s = connected(NavMode::Navigating);
  s.nav.maneuver = m;
  s.nav.distanceToManeuverM = distance;
  setRoadName(s.nav, road);
  return s;
}

const char* roadOf(const char* raw) {
  static char out[kRoadNameMax + 1];
  const ScreenView v = buildView(navigating(Maneuver::Straight, 100, raw));
  strcpy(out, v.road);
  return out;
}

}  // namespace

TEST(text_width) {
  CHECK_INT(textWidth("", 1), 0);
  CHECK_INT(textWidth("A", 1), 5);
  CHECK_INT(textWidth("AB", 1), 11);
  CHECK_INT(textWidth("MOTONAV", 3), 123);  // fits a 128 px display
}

TEST(fit_text) {
  char out[40];
  fitText(out, sizeof(out), "MG Road", 21);
  CHECK_STR(out, "MG Road");

  fitText(out, sizeof(out), "ABCDEFGHIJKLMNOPQRSTU", 21);  // exactly 21
  CHECK_STR(out, "ABCDEFGHIJKLMNOPQRSTU");

  fitText(out, sizeof(out), "Sir Mokshagundam Visvesvaraya Road", 21);
  CHECK_STR(out, "Sir Mokshagundam V...");
  CHECK_INT(strlen(out), 21);

  fitText(out, sizeof(out), "ABCDEFGHIJ", 5);  // too small for "..."
  CHECK_STR(out, "ABCDE");

  fitText(out, 8, "ABCDEFGHIJ", 21);  // destination smaller than maxChars
  CHECK_INT(strlen(out), 7);
  CHECK_STR(out, "ABCD...");

  fitText(out, 0, "X", 5);  // must not touch the buffer
}

TEST(view_screen_selection) {
  CHECK(buildView(DeviceState()).screen == Screen::Disconnected);
  CHECK(buildView(connected(NavMode::Ready)).screen == Screen::Ready);
  CHECK(buildView(connected(NavMode::Rerouting)).screen == Screen::Rerouting);
  CHECK(buildView(connected(NavMode::Arrived)).screen == Screen::Arrived);
  CHECK(buildView(navigating(Maneuver::TurnLeft, 450, "X")).screen == Screen::Navigating);
}

TEST(view_disconnected_wins_over_stale_navigation) {
  DeviceState s = navigating(Maneuver::TurnLeft, 450, "Main Street");
  s.link = Link::Disconnected;
  const ScreenView v = buildView(s);
  CHECK(v.screen == Screen::Disconnected);
  CHECK(v == buildView(DeviceState()));
}

TEST(view_navigating_fields) {
  const ScreenView v = buildView(navigating(Maneuver::SharpRight, 1200, "Main Street"));
  CHECK(v.maneuver == Maneuver::SharpRight);
  CHECK_STR(v.distance.value, "1.2");
  CHECK_STR(v.distance.unit, "km");
  CHECK_STR(v.road, "Main Street");
}

TEST(view_roundabout_exit_only_for_roundabouts) {
  DeviceState s = navigating(Maneuver::Roundabout, 100, "X");
  s.nav.roundaboutExit = 3;
  CHECK_INT(buildView(s).roundaboutExit, 3);
  s.nav.maneuver = Maneuver::TurnLeft;
  CHECK_INT(buildView(s).roundaboutExit, 0);
}

TEST(view_road_name_cleanup) {
  CHECK_STR(roadOf("Main Street"), "Main Street");
  CHECK_STR(roadOf("  Main Street  "), "Main Street");
  CHECK_STR(roadOf("\tMain"), "Main");
  CHECK_STR(roadOf(""), "");

  // Non-ASCII: one '?' per run, and a name with nothing readable is dropped.
  CHECK_STR(roadOf("Caf\xC3\xA9 Road"), "Caf? Road");
  CHECK_STR(roadOf("\xE0\xB4\xAE\xE0\xB4\xB2 Road"), "? Road");              // Malayalam letters
  CHECK_STR(roadOf("\xE0\xB4\xAE\xE0\xB4\xB2\xE0\xB4\xAA\xE0\xB5\x8D"), "");  // only Malayalam
  CHECK_STR(roadOf("\x80" "ABC"), "?ABC");                                    // stray byte
  CHECK_STR(roadOf("Road \xE0\xB4"), "Road ?");                               // cut mid-character
  CHECK_STR(roadOf("???"), "");                                               // no letters or digits
}

TEST(view_equality_ignores_invisible_changes) {
  // 52 m and 54 m both display as "50 m": no visible change.
  CHECK(buildView(navigating(Maneuver::TurnLeft, 52, "A")) ==
        buildView(navigating(Maneuver::TurnLeft, 54, "A")));
  // 54 m -> 56 m changes the text (50 -> 60).
  CHECK(buildView(navigating(Maneuver::TurnLeft, 54, "A")) !=
        buildView(navigating(Maneuver::TurnLeft, 56, "A")));
  CHECK(buildView(navigating(Maneuver::TurnLeft, 54, "A")) !=
        buildView(navigating(Maneuver::TurnLeft, 54, "B")));
  CHECK(buildView(navigating(Maneuver::TurnLeft, 54, "A")) !=
        buildView(navigating(Maneuver::TurnRight, 54, "A")));

  // Ready ignores leftover navigation fields.
  DeviceState a = connected(NavMode::Ready);
  DeviceState b = a;
  b.nav.maneuver = Maneuver::UTurnLeft;
  b.nav.distanceToManeuverM = 5;
  setRoadName(b.nav, "Stale");
  CHECK(buildView(a) == buildView(b));
}

TEST(instruction_text_fits_the_panel) {
  // The widest panel text must fit the navigation panel of a 128 px display.
  const int panel = navPanelWidth(128);
  for (int i = 0; i < kManeuverCount; ++i) {
    const char* text = instructionText(static_cast<Maneuver>(i));
    CHECK(text[0] != '\0');
    CHECK(textWidth(text, 1) <= panel);
  }
}
