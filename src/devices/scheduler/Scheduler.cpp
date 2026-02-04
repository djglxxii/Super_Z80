#include "devices/scheduler/Scheduler.h"

#include "core/types.h"

namespace sz::scheduler {

void Scheduler::Reset() {
  scanline_ = 0;
  frame_ = 0;
}

void Scheduler::BeginFrame() {
  scanline_ = 0;
}

void Scheduler::EndFrame() {
  ++frame_;
}

int Scheduler::GetScanline() const {
  return scanline_;
}

u64 Scheduler::GetFrame() const {
  return frame_;
}

void Scheduler::StepScanline() {
  ++scanline_;
  if (scanline_ >= kTotalScanlines) {
    scanline_ = 0;
  }
}

int Scheduler::ComputeCpuBudgetTstatesForScanline() const {
  return cpu_budget_tstates_;
}

DebugState Scheduler::GetDebugState() const {
  DebugState state;
  state.scanline = scanline_;
  state.frame = frame_;
  state.cpu_budget_tstates = cpu_budget_tstates_;
  return state;
}

}  // namespace sz::scheduler
