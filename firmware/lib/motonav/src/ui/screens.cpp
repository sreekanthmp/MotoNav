#include "ui/screens.h"

#include "ui/icons.h"
#include "ui/text.h"

namespace motonav {

namespace {

constexpr int kRoomyMinHeight = 48;  // below this the compact layouts are used

bool roomy(const Canvas& c) { return c.height() >= kRoomyMinHeight; }

void drawCentered(Canvas& c, int y, const char* text, int scale) {
  const int x = (c.width() - textWidth(text, scale)) / 2;
  c.drawText(x < 0 ? 0 : x, y, text, scale, true);
}

// Navigation screen geometry, derived from the canvas size.
struct NavLayout {
  Box arrow;
  int panelX;
  int distY;
  int unitY;
  int instrY;
  int roadY;
  int distScale;
  int unitScale;
  bool unitBeside;  // unit to the right of the number (compact) or below it
  bool showInstruction;
  bool showRoad;
};

NavLayout makeNavLayout(int w, int h) {
  NavLayout l;
  l.showRoad = h >= kRoomyMinHeight;
  const int roadStrip = l.showRoad ? kGlyphCellH : 0;
  const int availH = h - (l.showRoad ? roadStrip + 1 : 0);

  int side = w * 2 / 5;
  if (side > availH) side = availH;
  l.arrow = Box{0, (availH - side) / 2, side, side};
  l.panelX = side + 3;
  l.roadY = h - kGlyphCellH;

  if (availH >= kRoomyMinHeight) {
    l.distScale = 3;
    l.unitScale = 2;
    l.distY = 1;
    l.unitY = l.distY + kGlyphCellH * l.distScale + 1;
    l.instrY = l.unitY + kGlyphCellH * l.unitScale + 2;
    l.unitBeside = false;
    l.showInstruction = true;
  } else {
    // Compact: a large number with a smaller unit beside it, bottoms aligned.
    l.distScale = 3;
    l.unitScale = 2;
    l.distY = (h - kGlyphCellH * l.distScale) / 2;
    l.unitY = l.distY + 7 * (l.distScale - l.unitScale);
    l.instrY = 0;
    l.unitBeside = true;
    l.showInstruction = false;
  }
  return l;
}

}  // namespace

int navPanelWidth(int canvasWidth) {
  const int side = canvasWidth * 2 / 5;
  return canvasWidth - (side + 3) - 1;
}

void drawDisconnectedScreen(Canvas& c) {
  if (roomy(c)) {
    drawCentered(c, 6, "MOTONAV", 3);
    drawCentered(c, 40, "BLUETOOTH", 1);
    drawCentered(c, 50, "DISCONNECTED", 1);
  } else {
    drawCentered(c, 1, "MOTONAV", 2);
    drawCentered(c, 22, "BT DISCONNECTED", 1);
  }
}

void drawReadyScreen(Canvas& c) {
  if (roomy(c)) {
    drawCentered(c, 6, "MOTONAV", 3);
    drawCentered(c, 40, "BLUETOOTH CONNECTED", 1);
    drawCentered(c, 50, "READY TO NAVIGATE", 1);
  } else {
    drawCentered(c, 1, "MOTONAV", 2);
    drawCentered(c, 22, "CONNECTED - READY", 1);
  }
}

void drawNavigatingScreen(Canvas& c, const ScreenView& v) {
  const NavLayout l = makeNavLayout(c.width(), c.height());

  drawManeuverIcon(c, l.arrow, v.maneuver, v.roundaboutExit);

  c.drawText(l.panelX, l.distY, v.distance.value, l.distScale, true);
  if (v.distance.unit[0] != '\0') {
    const int unitX = l.unitBeside
                          ? l.panelX + textWidth(v.distance.value, l.distScale) + 3
                          : l.panelX;
    c.drawText(unitX, l.unitY, v.distance.unit, l.unitScale, true);
  }

  if (l.showInstruction) {
    c.drawText(l.panelX, l.instrY, instructionText(v.maneuver), 1, true);
  }

  if (l.showRoad && v.road[0] != '\0') {
    char fitted[kRoadNameMax + 1];
    fitText(fitted, sizeof(fitted), v.road,
            static_cast<size_t>(c.width() / kGlyphCellW));
    drawCentered(c, l.roadY, fitted, 1);
  }
}

void drawReroutingScreen(Canvas& c) {
  if (roomy(c)) {
    drawCentered(c, 16, "REROUTING", 2);
    drawCentered(c, 40, "FINDING NEW ROUTE", 1);
  } else {
    drawCentered(c, 8, "REROUTING", 2);
  }
}

void drawArrivedScreen(Canvas& c) {
  if (roomy(c)) {
    const int iconSide = c.height() - 2 * kGlyphCellH - 4;
    drawManeuverIcon(c, Box{(c.width() - iconSide) / 2, 0, iconSide, iconSide},
                     Maneuver::Arrived, 0);
    drawCentered(c, c.height() - kGlyphCellH - 2, "DESTINATION REACHED", 1);
  } else {
    const int iconSide = c.height();
    drawManeuverIcon(c, Box{0, 0, iconSide, iconSide}, Maneuver::Arrived, 0);
    // Centre the text in the space to the right of the icon.
    const int free = c.width() - iconSide;
    c.drawText(iconSide + (free - textWidth("ARRIVED", 2)) / 2, 8, "ARRIVED", 2, true);
  }
}

void renderScreen(Canvas& canvas, const ScreenView& view) {
  canvas.clear();
  switch (view.screen) {
    case Screen::Disconnected: drawDisconnectedScreen(canvas); break;
    case Screen::Ready:        drawReadyScreen(canvas); break;
    case Screen::Navigating:   drawNavigatingScreen(canvas, view); break;
    case Screen::Rerouting:    drawReroutingScreen(canvas); break;
    case Screen::Arrived:      drawArrivedScreen(canvas); break;
  }
}

}  // namespace motonav
