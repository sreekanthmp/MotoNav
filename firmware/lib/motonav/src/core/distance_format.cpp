#include "core/distance_format.h"

#include <stdio.h>
#include <string.h>

namespace motonav {

namespace {

void setUnit(DistanceText& d, const char* unit) {
  strncpy(d.unit, unit, sizeof(d.unit) - 1);
  d.unit[sizeof(d.unit) - 1] = '\0';
}

void setNumber(DistanceText& d, unsigned value) {
  snprintf(d.value, sizeof(d.value), "%u", value);
}

}  // namespace

DistanceText formatDistance(uint32_t meters) {
  DistanceText d;

  if (meters == kDistanceUnknown) {
    strncpy(d.value, "--", sizeof(d.value) - 1);
    return d;
  }

  // Clamp first so the arithmetic below cannot overflow.
  if (meters >= 999500u) {
    setNumber(d, 999);
    setUnit(d, "km");
    return d;
  }

  if (meters < 975u) {
    const uint32_t rounded =
        meters < 100u ? ((meters + 5u) / 10u) * 10u : ((meters + 25u) / 50u) * 50u;
    setNumber(d, static_cast<unsigned>(rounded));
    setUnit(d, "m");
    return d;
  }

  setUnit(d, "km");
  if (meters < 9950u) {
    const unsigned tenths = static_cast<unsigned>((meters + 50u) / 100u);
    snprintf(d.value, sizeof(d.value), "%u.%u", tenths / 10u, tenths % 10u);
  } else {
    setNumber(d, static_cast<unsigned>((meters + 500u) / 1000u));
  }
  return d;
}

}  // namespace motonav
