#include "devices/input/InputController.h"

namespace sz::input {

void InputController::Reset() {
  buttons_ = HostButtons{};
}

void InputController::SetHostButtons(const HostButtons& buttons) {
  buttons_ = buttons;
}

DebugState InputController::GetDebugState() const {
  DebugState state;
  state.buttons = buttons_;
  return state;
}

}  // namespace sz::input
