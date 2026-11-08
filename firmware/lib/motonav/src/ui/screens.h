// One drawing function per screen. Each takes the canvas (and, for
// navigation, the view) and draws into it. They never call present() and know
// nothing about BLE, timing or the SSD1306.
//
// Layout is computed from canvas.width() / canvas.height(). Two shapes are
// handled: height >= 48 (for example 128x64) and a compact one below that
// (for example 128x32). Other sizes, including a round display, need their own
// layout and have not been verified.
#pragma once

#include "ui/canvas.h"
#include "ui/view.h"

namespace motonav {

void drawDisconnectedScreen(Canvas& canvas);
void drawReadyScreen(Canvas& canvas);
void drawNavigatingScreen(Canvas& canvas, const ScreenView& view);
void drawReroutingScreen(Canvas& canvas);
void drawArrivedScreen(Canvas& canvas);

// Clears the canvas and draws the screen the view asks for.
void renderScreen(Canvas& canvas, const ScreenView& view);

// Width in pixels available for the maneuver instruction on the navigation
// screen of a canvas this wide. Exposed so tests can prove the text fits.
int navPanelWidth(int canvasWidth);

}  // namespace motonav
