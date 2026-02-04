#ifndef SUPERZ80_CPU_Z80CPUSTUB_H
#define SUPERZ80_CPU_Z80CPUSTUB_H

#include "core/types.h"

namespace sz::cpu {

struct DebugState {
  u32 last_budget = 0;
  u64 total_tstates_executed = 0;
  bool int_line = false;
};

class Z80CpuStub {
 public:
  void Reset();
  u32 Step(u32 tstates_budget);  // returns actual T-states executed
  DebugState GetDebugState() const;

  // Phase 4: Set external /INT line (level-sensitive)
  void SetIntLine(bool asserted) { int_line_ = asserted; }

 private:
  u32 last_budget_ = 0;
  u64 total_tstates_executed_ = 0;
  bool int_line_ = false;  // Phase 4: external interrupt line
};

}  // namespace sz::cpu

#endif
