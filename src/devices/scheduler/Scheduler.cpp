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
  vblank_flag_ = false;
  cpu_cycle_accumulator_ = 0.0;
  cycles_this_scanline_ = 0;
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

  RecordScanlineInRingBuffer(acc_before, cpu_cycle_accumulator_, cycles);
  return cycles;
}

void Scheduler::RecordScanlineInRingBuffer(double acc_before, double acc_after, u32 cycles) {
  ScanlineRecord& rec = ring_buffer_[ring_buffer_head_];
  rec.frame_counter_at_time = frame_counter_;
  rec.scanline = current_scanline_;
  rec.vblank_flag = vblank_flag_;
  rec.cycles_this_scanline = cycles;
  rec.accumulator_before = acc_before;
  rec.accumulator_after = acc_after;

  ring_buffer_head_ = (ring_buffer_head_ + 1) % kRingBufferSize;
}

void Scheduler::StepOneScanline(sz::console::SuperZ80Console& console) {
  // Per-scanline execution order (architecture contract):
  // 1. Scheduler computes cycles_this_line using accumulator rule
  cycles_this_scanline_ = ComputeCyclesThisLine();

  // 2. CPU executes for exactly cycles_this_line T-states
  console.ExecuteCpu(cycles_this_scanline_);

  // 3. IRQController updates /INT line (Phase 3: must remain deasserted)
  console.TickIRQ();

  // 4. If visible scanline (0-191): PPU hook (stub in Phase 3)
  if (current_scanline_ <= 191) {
    console.OnVisibleScanline(current_scanline_);
  }

  // 5. If scanline == 192: enter VBlank (no IRQ in Phase 3)
  if (current_scanline_ == kVBlankStartScanline) {
    vblank_flag_ = true;
  }

  // 6. DMAEngine tick stub (no-op in Phase 3)
  console.TickDMA();

  // 7. APU tick stub (no-op in Phase 3)
  console.TickAPU(cycles_this_scanline_);

  // 8. Advance scanline
  current_scanline_++;
  if (current_scanline_ >= kTotalScanlines) {
    current_scanline_ = 0;
    frame_counter_++;
    vblank_flag_ = false;  // clear vblank at scanline 0
  }

  // Cycle accounting invariant check (debug builds)
  #ifndef NDEBUG
  u64 lines_executed_total = frame_counter_ * kTotalScanlines + current_scanline_;
  double expected_cycles = static_cast<double>(lines_executed_total) * kCpuCyclesPerLine;
  double actual_cycles = static_cast<double>(total_cpu_tstates_executed_) + cpu_cycle_accumulator_;
  double error = std::abs(expected_cycles - actual_cycles);
  double epsilon = 1e-6 * static_cast<double>(lines_executed_total);
  SZ_ASSERT(error < epsilon || epsilon < 1e-9);  // allow for initial scanlines
  #endif
}

void Scheduler::StepOneFrame(sz::console::SuperZ80Console& console) {
  for (int i = 0; i < kTotalScanlines; ++i) {
    StepOneScanline(console);
  }
}

void Scheduler::RecordCpuTStatesExecuted(u32 tstates) {
  total_cpu_tstates_executed_ += tstates;
}

DebugState Scheduler::GetDebugState() const {
  DebugState state;
  state.current_scanline = current_scanline_;
  state.frame_counter = frame_counter_;
  state.vblank_flag = vblank_flag_;
  state.cycles_this_scanline = cycles_this_scanline_;
  state.cpu_cycle_accumulator = cpu_cycle_accumulator_;
  state.total_cpu_tstates_executed = total_cpu_tstates_executed_;
  state.cpu_cycles_per_line = kCpuCyclesPerLine;
  return state;
}

}  // namespace sz::scheduler
