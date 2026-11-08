// Navigation model and distance formatting.
#include <stdlib.h>
#include <string.h>

#include "core/distance_format.h"
#include "core/nav_model.h"
#include "test_framework.h"

using namespace motonav;

TEST(distance_boundaries) {
  struct Case {
    uint32_t meters;
    const char* value;
    const char* unit;
  };
  const Case cases[] = {
      {0, "0", "m"},        {4, "0", "m"},        {5, "10", "m"},      {14, "10", "m"},
      {15, "20", "m"},      {94, "90", "m"},      {95, "100", "m"},    {99, "100", "m"},
      {100, "100", "m"},    {124, "100", "m"},    {125, "150", "m"},   {449, "450", "m"},
      {450, "450", "m"},    {474, "450", "m"},    {475, "500", "m"},   {949, "950", "m"},
      {974, "950", "m"},    {975, "1.0", "km"},   {999, "1.0", "km"},  {1000, "1.0", "km"},
      {1049, "1.0", "km"},  {1050, "1.1", "km"},  {1200, "1.2", "km"}, {9949, "9.9", "km"},
      {9950, "10", "km"},   {9999, "10", "km"},   {10000, "10", "km"}, {10499, "10", "km"},
      {10500, "11", "km"},  {12000, "12", "km"},  {999499, "999", "km"},
      {999500, "999", "km"}, {4294967294u, "999", "km"},
  };
  for (const Case& c : cases) {
    const DistanceText d = formatDistance(c.meters);
    if (strcmp(d.value, c.value) != 0 || strcmp(d.unit, c.unit) != 0) {
      printf("    distance %u -> \"%s %s\", expected \"%s %s\"\n",
             static_cast<unsigned>(c.meters), d.value, d.unit, c.value, c.unit);
    }
    CHECK_STR(d.value, c.value);
    CHECK_STR(d.unit, c.unit);
  }
}

TEST(distance_unknown) {
  const DistanceText d = formatDistance(kDistanceUnknown);
  CHECK_STR(d.value, "--");
  CHECK_STR(d.unit, "");
}

// Over the whole realistic range the shown value never goes backwards as the
// real distance grows, and never strays from the truth by more than half a
// display step.
TEST(distance_monotonic_and_accurate) {
  double previous = -1.0;
  for (uint32_t m = 0; m <= 30000; ++m) {
    const DistanceText d = formatDistance(m);
    CHECK(strlen(d.value) <= 7 && strlen(d.unit) <= 2);
    const double shown = atof(d.value) * (strcmp(d.unit, "km") == 0 ? 1000.0 : 1.0);
    if (shown < previous) {
      printf("    shown value went backwards at %u m\n", static_cast<unsigned>(m));
      CHECK(shown >= previous);
      return;
    }
    previous = shown;

    double halfStep = 25.0;  // 100..974 m, 50 m steps
    if (m < 100) halfStep = 5.0;
    else if (m >= 975 && m < 9950) halfStep = 50.0;
    else if (m >= 9950) halfStep = 500.0;
    const double err = shown > m ? shown - m : m - shown;
    if (err > halfStep) {
      printf("    %u m shown as %s %s (error %.0f m)\n", static_cast<unsigned>(m), d.value,
             d.unit, err);
      CHECK(err <= halfStep);
      return;
    }
  }
  CHECK(true);
}

TEST(road_name_truncates_and_terminates) {
  NavSnapshot s;
  setRoadName(s, "Sir Mokshagundam Visvesvaraya Road");  // 34 chars, keeps the first 31
  CHECK_INT(strlen(s.roadName), kRoadNameMax);
  CHECK_STR(s.roadName, "Sir Mokshagundam Visvesvaraya R");

  setRoadName(s, "MG Road");
  CHECK_STR(s.roadName, "MG Road");

  setRoadName(s, nullptr);
  CHECK_STR(s.roadName, "");
}

TEST(snapshot_defaults_are_safe) {
  const DeviceState d;
  CHECK(d.link == Link::Disconnected);
  CHECK(d.nav.mode == NavMode::Ready);
  CHECK(d.nav.distanceToManeuverM == kDistanceUnknown);
  CHECK_STR(d.nav.roadName, "");
}
