#include "ui/icons.h"

#include <math.h>

namespace motonav {

namespace {

// Direction vectors and arc points are fixed constants (not sinf/cosf) so
// the drawing is identical on the device and on the host.
constexpr float kD45 = 0.70710678f;

struct IPt {
  int x;
  int y;
};

struct Frame {
  int cx;     // centre of the icon, pixels
  int cy;
  float r;    // half the icon size, pixels
  int t;      // line thickness, pixels
};

int iround(float v) { return static_cast<int>(lroundf(v)); }

// Design coordinates: origin at the centre, x to the right, y down, +-1 is
// the edge of the icon box.
IPt at(const Frame& f, float ux, float uy) {
  return IPt{f.cx + iround(ux * f.r), f.cy + iround(uy * f.r)};
}

void thickSegment(Canvas& c, IPt a, IPt b, int t) {
  const float dx = static_cast<float>(b.x - a.x);
  const float dy = static_cast<float>(b.y - a.y);
  const float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.5f) {
    c.fillCircle(a.x, a.y, t / 2, true);
    return;
  }
  const float nx = -dy / len * (static_cast<float>(t) * 0.5f);
  const float ny = dx / len * (static_cast<float>(t) * 0.5f);
  const IPt p0{iround(static_cast<float>(a.x) + nx), iround(static_cast<float>(a.y) + ny)};
  const IPt p1{iround(static_cast<float>(a.x) - nx), iround(static_cast<float>(a.y) - ny)};
  const IPt p2{iround(static_cast<float>(b.x) - nx), iround(static_cast<float>(b.y) - ny)};
  const IPt p3{iround(static_cast<float>(b.x) + nx), iround(static_cast<float>(b.y) + ny)};
  c.fillTriangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, true);
  c.fillTriangle(p0.x, p0.y, p2.x, p2.y, p3.x, p3.y, true);
}

// Polyline of thick segments with round joints and an arrowhead at the last
// point. `pts` has n >= 2 entries.
void arrowPolyline(Canvas& c, const Frame& f, const IPt* pts, int n) {
  const int t = f.t;
  const float headHalf = static_cast<float>(t) * 1.6f;
  const float headLen = headHalf * 1.15f;

  const float dx = static_cast<float>(pts[n - 1].x - pts[n - 2].x);
  const float dy = static_cast<float>(pts[n - 1].y - pts[n - 2].y);
  const float len = sqrtf(dx * dx + dy * dy);
  const float ux = len > 0.0f ? dx / len : 0.0f;
  const float uy = len > 0.0f ? dy / len : -1.0f;

  const float tipX = static_cast<float>(pts[n - 1].x);
  const float tipY = static_cast<float>(pts[n - 1].y);
  const float baseX = tipX - ux * headLen;
  const float baseY = tipY - uy * headLen;
  const IPt base{iround(baseX), iround(baseY)};

  for (int i = 0; i < n - 1; ++i) {
    const IPt a = pts[i];
    const IPt b = (i == n - 2) ? base : pts[i + 1];
    thickSegment(c, a, b, t);
    if (i > 0) c.fillCircle(a.x, a.y, t / 2, true);
  }

  const float px = -uy * headHalf;
  const float py = ux * headHalf;
  c.fillTriangle(pts[n - 1].x, pts[n - 1].y,
                 iround(baseX + px), iround(baseY + py),
                 iround(baseX - px), iround(baseY - py), true);
}

void straightArrow(Canvas& c, const Frame& f) {
  const IPt pts[2] = {at(f, 0.0f, 0.95f), at(f, 0.0f, -0.95f)};
  arrowPolyline(c, f, pts, 2);
}

// A stem from the bottom, a bend, then a second leg in the unit direction
// (legX, legY), y down, mirrored by `side` (-1 left, +1 right). The stem is
// moved `stemOffset` away from the turn side so the whole icon is centred and
// the arrowhead has room without touching the stem.
void bentArrow(Canvas& c, const Frame& f, float side, float stemOffset,
               float bendY, float legX, float legY, float legLen) {
  const float stemX = -side * stemOffset;
  const IPt pts[3] = {
      at(f, stemX, 0.95f),
      at(f, stemX, bendY),
      at(f, stemX + side * legX * legLen, bendY + legY * legLen),
  };
  arrowPolyline(c, f, pts, 3);
}

void uTurnArrow(Canvas& c, const Frame& f, float side) {
  // Semicircle over the top, as 22.5 degree steps. Left U-turn (side = -1) goes
  // up on the right and comes down on the left.
  static const float kCos[9] = {1.0f, 0.92388f, kD45, 0.38268f, 0.0f,
                                -0.38268f, -kD45, -0.92388f, -1.0f};
  static const float kSin[9] = {0.0f, 0.38268f, kD45, 0.92388f, 1.0f,
                                0.92388f, kD45, 0.38268f, 0.0f};
  const float radius = 0.40f;
  const float arcY = -0.22f;
  const float s = -side;  // mirror: left U-turn starts on the right

  IPt pts[12];
  int n = 0;
  pts[n++] = at(f, s * radius, 0.95f);
  for (int i = 0; i < 9; ++i) {
    pts[n++] = at(f, s * radius * kCos[i], arcY - radius * kSin[i]);
  }
  pts[n++] = at(f, -s * radius, 0.55f);
  arrowPolyline(c, f, pts, n);
}

void roundaboutIcon(Canvas& c, const Frame& f, uint8_t exitNumber) {
  const int ringCy = f.cy + iround(0.12f * f.r);
  const int outer = iround(0.58f * f.r);
  const int ringT = f.t > 4 ? f.t - 2 : f.t;
  const int inner = outer - ringT;

  c.fillCircle(f.cx, ringCy, outer, true);
  c.fillCircle(f.cx, ringCy, inner, false);

  // Entry stem from the bottom edge of the box into the ring.
  thickSegment(c, at(f, 0.0f, 0.95f), IPt{f.cx, ringCy + outer - ringT / 2}, f.t);

  // Exit arrow leaving the top of the ring.
  const IPt exitPts[2] = {IPt{f.cx, ringCy - outer + ringT}, at(f, 0.0f, -0.95f)};
  arrowPolyline(c, f, exitPts, 2);

  if (exitNumber >= 1 && exitNumber <= 9) {
    char digit[2] = {static_cast<char>('0' + exitNumber), '\0'};
    const int scale = inner >= 11 ? 2 : 1;
    const int w = textWidth(digit, scale);
    const int h = 7 * scale;
    c.drawText(f.cx - w / 2, ringCy - h / 2, digit, scale, true);
  }
}

void destinationPin(Canvas& c, const Frame& f) {
  const IPt head = at(f, 0.0f, -0.35f);
  const int headR = iround(0.50f * f.r);
  c.fillCircle(head.x, head.y, headR, true);
  c.fillTriangle(at(f, -0.46f, -0.12f).x, at(f, -0.46f, -0.12f).y,
                 at(f, 0.46f, -0.12f).x, at(f, 0.46f, -0.12f).y,
                 at(f, 0.0f, 0.95f).x, at(f, 0.0f, 0.95f).y, true);
  c.fillCircle(head.x, head.y, iround(0.20f * f.r), false);
}

}  // namespace

void drawManeuverIcon(Canvas& canvas, const Box& box, Maneuver m,
                      uint8_t roundaboutExit) {
  const int size = box.w < box.h ? box.w : box.h;
  Frame f;
  f.cx = box.x + box.w / 2;
  f.cy = box.y + box.h / 2;
  f.r = static_cast<float>(size) * 0.5f;
  const int t = iround(static_cast<float>(size) * 0.14f);
  f.t = t < 2 ? 2 : t;

  switch (m) {
    case Maneuver::Straight:
      straightArrow(canvas, f);
      break;
    case Maneuver::TurnLeft:
      bentArrow(canvas, f, -1.0f, 0.32f, 0.12f, 1.0f, 0.0f, 1.05f);
      break;
    case Maneuver::TurnRight:
      bentArrow(canvas, f, 1.0f, 0.32f, 0.12f, 1.0f, 0.0f, 1.05f);
      break;
    case Maneuver::SlightLeft:
      bentArrow(canvas, f, -1.0f, 0.25f, 0.30f, kD45, -kD45, 1.25f);
      break;
    case Maneuver::SlightRight:
      bentArrow(canvas, f, 1.0f, 0.25f, 0.30f, kD45, -kD45, 1.25f);
      break;
    case Maneuver::SharpLeft:
      bentArrow(canvas, f, -1.0f, 0.45f, -0.55f, kD45, kD45, 1.30f);
      break;
    case Maneuver::SharpRight:
      bentArrow(canvas, f, 1.0f, 0.45f, -0.55f, kD45, kD45, 1.30f);
      break;
    case Maneuver::UTurnLeft:
      uTurnArrow(canvas, f, -1.0f);
      break;
    case Maneuver::UTurnRight:
      uTurnArrow(canvas, f, 1.0f);
      break;
    case Maneuver::Roundabout:
      roundaboutIcon(canvas, f, roundaboutExit);
      break;
    case Maneuver::Arrived:
      destinationPin(canvas, f);
      break;
  }
}

}  // namespace motonav
