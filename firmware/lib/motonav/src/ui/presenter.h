// Decides when to redraw and push a frame. This is what keeps the display
// from flickering: a frame is rendered and presented only when something
// visible changed.
#pragma once

#include "core/nav_model.h"
#include "ui/canvas.h"
#include "ui/view.h"

namespace motonav {

class Presenter {
 public:
  explicit Presenter(Canvas& canvas) : canvas_(canvas) {}

  // Returns true if a new frame was rendered and presented.
  bool update(const DeviceState& state);

  // Forget what is on the panel, so the next update() always draws. Call this
  // after the display was (re)initialised.
  void invalidate() { hasLast_ = false; }

 private:
  Canvas& canvas_;
  ScreenView last_;
  bool hasLast_ = false;
};

}  // namespace motonav
