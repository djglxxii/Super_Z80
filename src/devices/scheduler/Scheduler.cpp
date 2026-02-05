#include "devices/scheduler/Scheduler.h"

#include <cmath>
#include "console/SuperZ80Console.h"
#include "core/log/Logger.h"
#include "core/types.h"
#include "core/util/Assert.h"

namespace sz::scheduler {

void Scheduler::Reset() {
  current_scanline_ = 0;
  frame_counter_ = 0;
  total_cpu_tstates_executed_ = 0;
  cpu_cycle_accumulator_ = 0.0;
  cycles_this_scanline_ = 0;
  cpu_cycle_debt_ = 0;
  ring_buffer_head_ = 0;
  ring_buffer_ = {};
}

u32 Scheduler::ComputeCyclesThisLine() {
  // Mandatory fractional accumulator rule:
  // 1. accumulator += cpu_cycles_per_line
  // 2. cycles_this_line = floor(accumulator)
  // 3. accumulator -= cycles_this_line

  double acc_before = cpu_cycle_accumulator_;
  cpu_cycle_accumulator_ += kCpuCyclesPerLine;
  u32 cycles = static_cast<u32>(std::floor(cpu_cycle_accumulator_));
  cpu_cycle_accumulator_ -= static_cast<double>(cycles);

  // Phase 9: Account for cycle debt from previous scanline
  // (CPU may have executed more than budgeted due to instruction boundaries)
  if (cpu_cycle_debt_ > 0 && cycles > static_cast<u32>(cpu_cycle_debt_)) {
    cycles -= static_cast<u32>(cpu_cycle_debt_);
    cpu_cycle_debt_ = 0;
  } else if (cpu_cycle_debt_ > 0) {
    // Debt exceeds this scanline's budget - carry over
    cpu_cycle_debt_ -= static_cast<s32>(cycles);
    cycles = 0;
  }

  RecordScanlineInRingBuffer(acc_before, cpu_cycle_accumulator_, cycles);
  return cycles;
}

void Scheduler::RecordScanlineInRingBuffer(double acc_before, double acc_after, u32 cycles) {
  ScanlineRecord& rec = ring_buffer_[ring_buffer_head_];
  rec.frame_counter_at_time = frame_counter_;
  rec.scanline = current_scanline_;
  rec.vblank_flag = false;  // Phase 5: VBlank moved to PPU; deprecated here
  rec.cycles_this_scanline = cycles;
  rec.accumulator_before = acc_before;
  rec.accumulator_after = acc_after;

  ring_buffer_head_ = (ring_buffer_head_ + 1) % kRingBufferSize;
}

void Scheduler::StepOneScanline(sz::console::SuperZ80Console& console) {
  // Per-scanline execution order (architecture contract):
  // 1. Scheduler computes cycles_this_line using accumulator rule
  cycles_this_scanline_ = ComputeCyclesThisLine();

  // 2. Phase 5: Scanline start hook (VBlank timing + IRQ trigger + PreCpuUpdate)
  console.OnScanlineStart(current_scanline_);

  // 3. CPU executes for exactly cycles_this_line T-states
  console.ExecuteCpu(cycles_this_scanline_);

  // 4. IRQController PostCpuUpdate (ensure ACK drops /INT immediately)
  console.TickIRQ();

  // 5. If visible scanline (0-191): PPU hook (stub rendering)
  if (current_scanline_ <= 191) {
    console.OnVisibleScanline(current_scanline_);
  }

  // 6. DMAEngine tick stub (no-op until Phase 6)
  console.TickDMA();

  // 7. APU tick stub (no-op until Phase 12)
  console.TickAPU(cycles_this_scanline_);

  // 8. Advance scanline
  current_scanline_++;
  if (current_scanline_ >= kTotalScanlines) {
    current_scanline_ = 0;
    frame_counter_++;
  }

  // Phase 9: Cycle accounting invariant check (debug builds)
  // Note: With real CPU execution, we may accumulate cycle debt due to
  // instruction boundaries. The invariant is relaxed to account for this.
  #ifndef NDEBUG
  u64 lines_executed_total = frame_counter_ * kTotalScanlines + current_scanline_;
  double expected_cycles = static_cast<double>(lines_executed_total) * kCpuCyclesPerLine;
  // Adjust actual cycles by subtracting pending debt (cycles borrowed from future)
  double actual_cycles = static_cast<double>(total_cpu_tstates_executed_) + cpu_cycle_accumulator_
                         - static_cast<double>(cpu_cycle_debt_);
  double error = std::abs(expected_cycles - actual_cycles);
  // Allow tolerance for fractional cycles plus instruction overshoot (max ~23 cycles per instruction)
  double epsilon = 1e-6 * static_cast<double>(lines_executed_total) + 30.0;
  SZ_ASSERT(error < epsilon || lines_executed_total < 10);  // allow for initial scanlines
  #endif
}

void Scheduler::StepOneFrame(sz::console::SuperZ80Console& console) {
  for (int i = 0; i < kTotalScanlines; ++i) {
    StepOneScanline(console);
  }
}

void Scheduler::RecordCpuTStatesExecuted(u32 tstates) {
  total_cpu_tstates_executed_ += tstates;

  // Phase 9: Track cycle debt (if CPU executed more than budgeted)
  if (tstates > cycles_this_scanline_) {
    cpu_cycle_debt_ += static_cast<s32>(tstates - cycles_this_scanline_);
  }
}

DebugState Scheduler::GetDebugState() const {
  DebugState state;
  state.current_scanline = current_scanline_;
  state.frame_counter = frame_counter_;
  state.vblank_flag = false;  // Phase 5: VBlank moved to PPU; deprecated here
  state.cycles_this_scanline = cycles_this_scanline_;
  state.cpu_cycle_accumulator = cpu_cycle_accumulator_;
  state.total_cpu_tstates_executed = total_cpu_tstates_executed_;
  state.cpu_cycles_per_line = kCpuCyclesPerLine;
  return state;
}

}  // namespace sz::scheduler
