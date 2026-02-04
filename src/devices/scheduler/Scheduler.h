#ifndef SUPERZ80_DEVICES_SCHEDULER_SCHEDULER_H
#define SUPERZ80_DEVICES_SCHEDULER_SCHEDULER_H

#include "core/types.h"

namespace sz::scheduler {

struct DebugState {
  int scanline = 0;
  u64 frame = 0;
  int cpu_budget_tstates = 0;
};

class Scheduler {
 public:
  void Reset();
  void BeginFrame();
  void EndFrame();
  int GetScanline() const;
  u64 GetFrame() const;
  void StepScanline();
  int ComputeCpuBudgetTstatesForScanline() const;
  DebugState GetDebugState() const;

 private:
  int scanline_ = 0;
  u64 frame_ = 0;
  int cpu_budget_tstates_ = 228;  // placeholder only
};

}  // namespace sz::scheduler

#endif
