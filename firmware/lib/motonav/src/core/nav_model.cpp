#include "core/nav_model.h"

namespace motonav {

void setRoadName(NavSnapshot& snapshot, const char* name) {
  size_t i = 0;
  if (name != nullptr) {
    for (; i < kRoadNameMax && name[i] != '\0'; ++i) {
      snapshot.roadName[i] = name[i];
    }
  }
  snapshot.roadName[i] = '\0';
}

const char* maneuverName(Maneuver m) {
  switch (m) {
    case Maneuver::Straight:    return "Straight";
    case Maneuver::TurnLeft:    return "TurnLeft";
    case Maneuver::TurnRight:   return "TurnRight";
    case Maneuver::SlightLeft:  return "SlightLeft";
    case Maneuver::SlightRight: return "SlightRight";
    case Maneuver::SharpLeft:   return "SharpLeft";
    case Maneuver::SharpRight:  return "SharpRight";
    case Maneuver::UTurnLeft:   return "UTurnLeft";
    case Maneuver::UTurnRight:  return "UTurnRight";
    case Maneuver::Roundabout:  return "Roundabout";
    case Maneuver::Arrived:     return "Arrived";
  }
  return "?";
}

}  // namespace motonav
