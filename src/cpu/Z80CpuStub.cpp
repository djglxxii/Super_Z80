#include "cpu/Z80CpuStub.h"

namespace sz::cpu {

void Z80CpuStub::Reset() {
  last_budget_ = 0;
  total_tstates_executed_ = 0;
}

u32 Z80CpuStub::Step(u32 tstates_budget) {
  // Stub: execute exactly the requested number of T-states
  // In Phase 3, this is used for timing validation
  last_budget_ = tstates_budget;
  total_tstates_executed_ += tstates_budget;
  return tstates_budget;  // report that we executed exactly what was requested
}

DebugState Z80CpuStub::GetDebugState() const {
  DebugState state;
  state.last_budget = last_budget_;
  state.total_tstates_executed = total_tstates_executed_;
  return state;
}

}  // namespace sz::cpu
