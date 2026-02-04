#include "cpu/Z80CpuStub.h"

namespace sz::cpu {

void Z80CpuStub::Reset() {
  last_budget_ = 0;
}

void Z80CpuStub::Step(int tstates_budget) {
  last_budget_ = tstates_budget;
}

DebugState Z80CpuStub::GetDebugState() const {
  DebugState state;
  state.last_budget = last_budget_;
  return state;
}

}  // namespace sz::cpu
