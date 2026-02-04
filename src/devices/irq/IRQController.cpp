#include "devices/irq/IRQController.h"

namespace sz::irq {

void IRQController::Reset() {
  int_asserted_ = false;
}

void IRQController::Tick() {
}

bool IRQController::IsIntAsserted() const {
  return int_asserted_;
}

DebugState IRQController::GetDebugState() const {
  DebugState state;
  state.int_asserted = int_asserted_;
  return state;
}

}  // namespace sz::irq
