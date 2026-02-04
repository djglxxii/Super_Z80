#include "devices/dma/DMAEngine.h"

namespace sz::dma {

void DMAEngine::Reset() {
  ticks_ = 0;
}

void DMAEngine::Tick() {
  ++ticks_;
}

DebugState DMAEngine::GetDebugState() const {
  DebugState state;
  state.ticks = ticks_;
  return state;
}

}  // namespace sz::dma
