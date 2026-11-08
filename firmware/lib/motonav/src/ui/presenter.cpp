#include "ui/presenter.h"

#include "ui/screens.h"

namespace motonav {

bool Presenter::update(const DeviceState& state) {
  const ScreenView view = buildView(state);
  if (hasLast_ && view == last_) return false;

  renderScreen(canvas_, view);
  canvas_.present();
  last_ = view;
  hasLast_ = true;
  return true;
}

}  // namespace motonav
