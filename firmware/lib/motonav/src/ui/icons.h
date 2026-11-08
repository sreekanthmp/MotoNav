// Maneuver icons, drawn as vector shapes (thick lines, triangles, circles).
// There are no stored bitmaps, so icons scale to whatever box they are given
// and are reused unchanged on a different display.
#pragma once

#include <stdint.h>

#include "core/nav_model.h"
#include "ui/canvas.h"

namespace motonav {

// Draws the icon for `m` centred in `box`, using the smaller of the box's
// width and height as its size. `roundaboutExit` (1..9) is drawn inside the
// roundabout ring; 0 or larger draws no number.
void drawManeuverIcon(Canvas& canvas, const Box& box, Maneuver m,
                      uint8_t roundaboutExit);

}  // namespace motonav
