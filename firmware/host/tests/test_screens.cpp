// Rendering checks. Everything here is HOST-verified: it proves layout,
// bounds, symmetry and distinctness on a bitmap, not how the physical
// display looks.
#include <math.h>
#include <stdlib.h>

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "bitmap_canvas.h"
#include "test_framework.h"
#include "ui/icons.h"
#include "ui/screens.h"

using namespace motonav;
using hosttest::BitmapCanvas;

namespace {

struct Size {
  int w, h;
};
const Size kSizes[] = {{128, 64}, {128, 32}};

DeviceState navigating(Maneuver m, uint32_t distance, const char* road, uint8_t exitNo = 2) {
  DeviceState s;
  s.link = Link::Connected;
  s.nav.mode = NavMode::Navigating;
  s.nav.maneuver = m;
  s.nav.distanceToManeuverM = distance;
  s.nav.roundaboutExit = exitNo;
  setRoadName(s.nav, road);
  return s;
}

DeviceState connected(NavMode mode) {
  DeviceState s;
  s.link = Link::Connected;
  s.nav.mode = mode;
  return s;
}

void render(BitmapCanvas& c, const DeviceState& s) { renderScreen(c, buildView(s)); }

std::vector<DeviceState> everyScreen() {
  std::vector<DeviceState> v;
  v.push_back(DeviceState());
  v.push_back(connected(NavMode::Ready));
  for (int i = 0; i < kManeuverCount; ++i) {
    v.push_back(navigating(static_cast<Maneuver>(i), 450, "Sir Mokshagundam Visvesvaraya Road"));
  }
  v.push_back(navigating(Maneuver::TurnRight, kDistanceUnknown, ""));
  v.push_back(navigating(Maneuver::TurnRight, 12000, "MG Road"));
  v.push_back(connected(NavMode::Rerouting));
  v.push_back(connected(NavMode::Arrived));
  return v;
}

// A square canvas holding one icon, for comparing icons with each other.
BitmapCanvas iconBitmap(Maneuver m, uint8_t exitNo, int side = 56) {
  BitmapCanvas c(side, side);
  drawManeuverIcon(c, Box{0, 0, side, side}, m, exitNo);
  return c;
}

int differingPixels(const BitmapCanvas& a, const BitmapCanvas& b) {
  int n = 0;
  for (int y = 0; y < a.height(); ++y)
    for (int x = 0; x < a.width(); ++x) n += a.pixel(x, y) != b.pixel(x, y);
  return n;
}

// Mismatch between `a` and the left-right mirror of `b` about the centre column.
int mirrorMismatch(const BitmapCanvas& a, const BitmapCanvas& b) {
  const int w = a.width();
  int n = 0;
  for (int y = 0; y < a.height(); ++y)
    for (int x = 0; x < w; ++x) {
      const int mx = w - x;  // centre column is w/2
      const bool other = (mx >= 0 && mx < w) ? b.pixel(mx, y) : false;
      n += a.pixel(x, y) != other;
    }
  return n;
}

}  // namespace

TEST(screens_stay_inside_the_canvas_and_draw_something) {
  for (const Size& size : kSizes) {
    for (const DeviceState& s : everyScreen()) {
      BitmapCanvas c(size.w, size.h);
      render(c, s);
      if (c.outOfBounds() != 0) {
        printf("    %dx%d screen %d drew %d pixels out of bounds\n", size.w, size.h,
               static_cast<int>(buildView(s).screen), c.outOfBounds());
      }
      CHECK_INT(c.outOfBounds(), 0);
      CHECK(c.inkCount() > 40);                              // not blank
      CHECK(c.inkCount() < size.w * size.h * 55 / 100);      // not a solid block
    }
  }
}

TEST(rendering_is_deterministic_and_leaves_no_residue) {
  BitmapCanvas a(128, 64), b(128, 64);
  render(a, navigating(Maneuver::TurnLeft, 450, "MG Road"));
  render(b, navigating(Maneuver::TurnLeft, 450, "MG Road"));
  CHECK(a.hash() == b.hash());

  // Drawing a different screen over a used canvas must equal drawing it fresh.
  render(b, navigating(Maneuver::Roundabout, 90, "A Very Long Road Name Here"));
  render(b, connected(NavMode::Rerouting));
  BitmapCanvas fresh(128, 64);
  render(fresh, connected(NavMode::Rerouting));
  CHECK(b.hash() == fresh.hash());
}

TEST(all_maneuver_icons_are_distinct) {
  std::map<uint64_t, int> seen;
  int minDifference = 1 << 30;
  std::vector<BitmapCanvas> icons;
  for (int i = 0; i < kManeuverCount; ++i) {
    icons.push_back(iconBitmap(static_cast<Maneuver>(i), 2));
    CHECK(icons.back().inkCount() > 150);
    CHECK_INT(icons.back().outOfBounds(), 0);
    CHECK(seen.insert(std::make_pair(icons.back().hash(), i)).second);
  }
  for (size_t i = 0; i < icons.size(); ++i)
    for (size_t j = i + 1; j < icons.size(); ++j) {
      const int d = differingPixels(icons[i], icons[j]);
      if (d < minDifference) minDifference = d;
    }
  printf("    (smallest difference between any two icons: %d pixels)\n", minDifference);
  // Icons that differ by only a handful of pixels would be hard to tell apart.
  CHECK(minDifference >= 150);
}

TEST(left_and_right_icons_mirror_each_other) {
  const Maneuver pairs[][2] = {{Maneuver::TurnLeft, Maneuver::TurnRight},
                               {Maneuver::SlightLeft, Maneuver::SlightRight},
                               {Maneuver::SharpLeft, Maneuver::SharpRight},
                               {Maneuver::UTurnLeft, Maneuver::UTurnRight}};
  for (const auto& p : pairs) {
    const BitmapCanvas left = iconBitmap(p[0], 0);
    const BitmapCanvas right = iconBitmap(p[1], 0);
    const int mismatch = mirrorMismatch(left, right);
    const int tolerance = left.inkCount() / 25;  // 4 %: rasteriser edge effects only
    if (mismatch > tolerance) {
      printf("    mirror mismatch %d > %d for maneuver %d\n", mismatch, tolerance,
             static_cast<int>(p[0]));
    }
    CHECK(mismatch <= tolerance);
  }
}

TEST(turn_icons_lean_towards_their_turn_side) {
  // Where the arrow enters (the bottom row) versus where the rest of its ink
  // is (everything above the bottom stem). For a left-type maneuver the body of
  // the arrow lies to the left of its entry point, and mirrored for right-type.
  // This holds for sharp turns and U-turns too, unlike a "reaches further"
  // test, because those fold back towards the stem.
  auto entryX = [](const BitmapCanvas& c) {
    double sum = 0;
    int n = 0;
    for (int x = 0; x < c.width(); ++x)
      if (c.pixel(x, c.height() - 2)) { sum += x; ++n; }
    return n ? sum / n : -1.0;
  };
  auto bodyX = [](const BitmapCanvas& c) {
    double sum = 0;
    int n = 0;
    for (int y = 0; y <= 40; ++y)
      for (int x = 0; x < c.width(); ++x)
        if (c.pixel(x, y)) { sum += x; ++n; }
    return n ? sum / n : -1.0;
  };
  const Maneuver pairs[][2] = {{Maneuver::TurnLeft, Maneuver::TurnRight},
                               {Maneuver::SlightLeft, Maneuver::SlightRight},
                               {Maneuver::SharpLeft, Maneuver::SharpRight},
                               {Maneuver::UTurnLeft, Maneuver::UTurnRight}};
  for (const auto& p : pairs) {
    const BitmapCanvas left = iconBitmap(p[0], 0);
    const BitmapCanvas right = iconBitmap(p[1], 0);
    CHECK(entryX(left) > 0 && bodyX(left) < entryX(left) - 4.0);
    CHECK(entryX(right) > 0 && bodyX(right) > entryX(right) + 4.0);
  }
  // Straight and roundabout enter and leave in line.
  CHECK(std::abs(bodyX(iconBitmap(Maneuver::Straight, 0)) -
                 entryX(iconBitmap(Maneuver::Straight, 0))) < 2.0);
}

TEST(roundabout_shows_its_exit_number) {
  const BitmapCanvas none = iconBitmap(Maneuver::Roundabout, 0);
  const BitmapCanvas two = iconBitmap(Maneuver::Roundabout, 2);
  const BitmapCanvas three = iconBitmap(Maneuver::Roundabout, 3);
  const BitmapCanvas ten = iconBitmap(Maneuver::Roundabout, 10);  // out of range: no number
  CHECK(none.hash() != two.hash());
  CHECK(two.hash() != three.hash());
  CHECK(none.hash() == ten.hash());
}

TEST(navigation_layout_distance_is_the_dominant_text) {
  for (const Size& size : kSizes) {
    BitmapCanvas c(size.w, size.h);
    render(c, navigating(Maneuver::TurnRight, 450, "MG Road"));
    const int panelX = size.w * 2 / 5 + 3;
    // The number (scale 3) is at least 21 ink rows tall; no other text is.
    int firstRow = -1, lastRow = -1;
    for (int y = 0; y < size.h; ++y) {
      if (c.inkIn(panelX, y, size.w, y + 1) > 0) {
        if (firstRow < 0) firstRow = y;
        lastRow = y;
        if (lastRow - firstRow + 1 >= 21) break;
      } else if (lastRow - firstRow + 1 < 21) {
        firstRow = -1;  // run broken before reaching number height
      }
    }
    CHECK(firstRow >= 0 && lastRow - firstRow + 1 >= 21);
  }
}

TEST(road_name_strip_is_empty_without_a_road) {
  BitmapCanvas withRoad(128, 64), without(128, 64);
  render(withRoad, navigating(Maneuver::Straight, 450, "MG Road"));
  render(without, navigating(Maneuver::Straight, 450, ""));
  CHECK(withRoad.inkIn(0, 56, 128, 64) > 0);
  CHECK_INT(without.inkIn(0, 56, 128, 64), 0);
}

TEST(compact_layout_has_no_road_strip) {
  BitmapCanvas c(128, 32);
  render(c, navigating(Maneuver::Straight, 450, "MG Road"));
  CHECK_INT(c.outOfBounds(), 0);
  // The number and unit are vertically centred (rows 4..24); the bottom rows to
  // the right of the icon stay empty because a 32 px display has no room for
  // the road name.
  CHECK_INT(c.inkIn(36, 26, 128, 32), 0);
}

TEST(arrived_text_does_not_overlap_the_icon_on_a_compact_display) {
  BitmapCanvas c(128, 32);
  renderScreen(c, buildView(connected(NavMode::Arrived)));
  // The icon occupies x < 32. The text is centred in the free area to its right.
  // A gap column between them proves no collision.
  int gapInk = 0;
  for (int x = 31; x <= 34; ++x) gapInk += c.inkIn(x, 0, x + 1, 32);
  CHECK_INT(gapInk, 0);
}

// ---------------------------------------------------------------------------
// Golden hashes: catch unintended changes to the rendering. Regenerate with
// `make golden-update` after a deliberate visual change, and look at the
// images (`make render`) before accepting.
// ---------------------------------------------------------------------------
TEST(golden_screens) {
  std::vector<std::pair<std::string, uint64_t>> actual;
  for (const Size& size : kSizes) {
    int index = 0;
    for (const DeviceState& s : everyScreen()) {
      BitmapCanvas c(size.w, size.h);
      render(c, s);
      char name[64];
      snprintf(name, sizeof(name), "%dx%d_%02d", size.w, size.h, index++);
      actual.push_back(std::make_pair(std::string(name), c.hash()));
    }
  }

  const char* path = "golden/screens.txt";
  if (getenv("GOLDEN_UPDATE") != nullptr) {
    FILE* f = fopen(path, "w");
    CHECK(f != nullptr);
    if (!f) return;
    for (const auto& e : actual) fprintf(f, "%s %016llx\n", e.first.c_str(),
                                         static_cast<unsigned long long>(e.second));
    fclose(f);
    printf("    golden file updated: %zu entries\n", actual.size());
    return;
  }

  std::map<std::string, uint64_t> expected;
  FILE* f = fopen(path, "r");
  if (!f) {
    printf("    missing %s (run `make golden-update`)\n", path);
    CHECK(f != nullptr);
    return;
  }
  char name[64];
  unsigned long long hash;
  while (fscanf(f, "%63s %llx", name, &hash) == 2) expected[name] = hash;
  fclose(f);

  CHECK_INT(expected.size(), actual.size());
  for (const auto& e : actual) {
    auto it = expected.find(e.first);
    const bool ok = it != expected.end() && it->second == e.second;
    if (!ok) printf("    golden mismatch: %s\n", e.first.c_str());
    CHECK(ok);
  }
}
