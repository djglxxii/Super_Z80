#include "devices/apu/APU.h"

namespace sz::apu {

void APU::Reset() {
  last_cpu_tstates_ = 0;
}

void APU::Tick(int cpu_tstates_elapsed) {
  last_cpu_tstates_ = cpu_tstates_elapsed;
}

DebugState APU::GetDebugState() const {
  DebugState state;
  state.last_cpu_tstates = last_cpu_tstates_;
  return state;
}

}  // namespace sz::apu
