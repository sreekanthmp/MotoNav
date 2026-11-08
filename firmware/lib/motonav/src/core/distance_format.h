// Turns a distance in meters into short, glanceable text.
#pragma once

#include <stdint.h>

#include "core/nav_model.h"

namespace motonav {

// A number and a unit are returned separately so the screen can draw the
// number large and the unit small.
struct DistanceText {
  char value[8] = {0};  // "90", "450", "1.2", "12", "--"
  char unit[3] = {0};   // "m", "km", or "" when unknown
};

// Rules (rounding is to nearest, halves up):
//   unknown          -> "--"
//   0..99 m          -> 10 m steps      "90 m"
//   100..974 m       -> 50 m steps      "450 m"
//   975 m..9949 m    -> km, 1 decimal   "1.2 km"
//   9950 m and up    -> whole km        "12 km", capped at "999 km"
DistanceText formatDistance(uint32_t meters);

}  // namespace motonav
