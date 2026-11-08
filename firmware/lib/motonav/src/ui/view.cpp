#include "ui/view.h"

#include <stdio.h>
#include <string.h>

namespace motonav {

namespace {

bool isAlnum(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// The built-in font only has printable ASCII. Anything else (for example
// Devanagari or Malayalam road names) becomes a single '?' per run, and a
// name with no letters or digits left is dropped rather than shown as "???".
void sanitizeRoad(const char* src, char* dst, size_t dstSize) {
  size_t out = 0;
  bool anyAlnum = false;
  bool lastWasMark = false;

  for (size_t i = 0; src[i] != '\0' && out + 1 < dstSize;) {
    const unsigned char c = static_cast<unsigned char>(src[i]);
    if (c >= 0x20 && c < 0x7F) {
      dst[out++] = static_cast<char>(c);
      anyAlnum = anyAlnum || isAlnum(static_cast<char>(c));
      lastWasMark = false;
      ++i;
    } else if (c < 0x80) {  // control character
      dst[out++] = ' ';
      lastWasMark = false;
      ++i;
    } else {  // UTF-8 lead or stray continuation byte: skip the whole sequence
      size_t len = 1;
      if (c >= 0xF0 && c < 0xF8) len = 4;
      else if (c >= 0xE0) len = 3;
      else if (c >= 0xC0) len = 2;
      ++i;
      for (size_t k = 1; k < len && (static_cast<unsigned char>(src[i]) & 0xC0) == 0x80; ++k) ++i;
      if (!lastWasMark) {
        dst[out++] = '?';
        lastWasMark = true;
      }
    }
  }
  dst[out] = '\0';

  if (!anyAlnum) {
    dst[0] = '\0';
    return;
  }

  // Trim spaces at both ends.
  size_t start = 0;
  while (dst[start] == ' ') ++start;
  size_t end = strlen(dst);
  while (end > start && dst[end - 1] == ' ') --end;
  memmove(dst, dst + start, end - start);
  dst[end - start] = '\0';
}

}  // namespace

bool operator==(const ScreenView& a, const ScreenView& b) {
  return a.screen == b.screen && a.maneuver == b.maneuver &&
         a.roundaboutExit == b.roundaboutExit &&
         strcmp(a.distance.value, b.distance.value) == 0 &&
         strcmp(a.distance.unit, b.distance.unit) == 0 &&
         strcmp(a.road, b.road) == 0;
}

ScreenView buildView(const DeviceState& state) {
  ScreenView v;

  if (state.link == Link::Disconnected) {
    v.screen = Screen::Disconnected;
    return v;
  }

  switch (state.nav.mode) {
    case NavMode::Ready:
      v.screen = Screen::Ready;
      break;
    case NavMode::Rerouting:
      v.screen = Screen::Rerouting;
      break;
    case NavMode::Arrived:
      v.screen = Screen::Arrived;
      break;
    case NavMode::Navigating:
      v.screen = Screen::Navigating;
      v.maneuver = state.nav.maneuver;
      v.roundaboutExit =
          state.nav.maneuver == Maneuver::Roundabout ? state.nav.roundaboutExit : 0;
      v.distance = formatDistance(state.nav.distanceToManeuverM);
      sanitizeRoad(state.nav.roadName, v.road, sizeof(v.road));
      break;
  }
  return v;
}

const char* screenName(Screen s) {
  switch (s) {
    case Screen::Disconnected: return "Disconnected";
    case Screen::Ready:        return "Ready";
    case Screen::Navigating:   return "Navigating";
    case Screen::Rerouting:    return "Rerouting";
    case Screen::Arrived:      return "Arrived";
  }
  return "?";
}

void describeView(const ScreenView& v, char* out, size_t outSize) {
  if (outSize == 0) return;
  if (v.screen != Screen::Navigating) {
    snprintf(out, outSize, "screen=%s", screenName(v.screen));
    return;
  }
  char exitText[16] = "";
  if (v.roundaboutExit != 0) snprintf(exitText, sizeof(exitText), " exit=%u", static_cast<unsigned>(v.roundaboutExit));
  snprintf(out, outSize, "screen=Navigating icon=%s%s distance=\"%s%s%s\" road=\"%s\"",
           maneuverName(v.maneuver), exitText, v.distance.value,
           v.distance.unit[0] != '\0' ? " " : "", v.distance.unit, v.road);
}

const char* instructionText(Maneuver m) {
  switch (m) {
    case Maneuver::Straight:    return "CONTINUE";
    case Maneuver::TurnLeft:    return "TURN LEFT";
    case Maneuver::TurnRight:   return "TURN RIGHT";
    case Maneuver::SlightLeft:  return "SLIGHT LEFT";
    case Maneuver::SlightRight: return "SLIGHT RIGHT";
    case Maneuver::SharpLeft:   return "SHARP LEFT";
    case Maneuver::SharpRight:  return "SHARP RIGHT";
    case Maneuver::UTurnLeft:
    case Maneuver::UTurnRight:  return "U-TURN";
    case Maneuver::Roundabout:  return "ROUNDABOUT";
    case Maneuver::Arrived:     return "DESTINATION";
  }
  return "";
}

}  // namespace motonav
