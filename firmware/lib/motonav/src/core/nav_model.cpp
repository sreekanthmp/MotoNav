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

}  // namespace motonav
