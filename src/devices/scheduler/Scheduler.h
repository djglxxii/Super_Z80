#ifndef SUPERZ80_DEVICES_SCHEDULER_SCHEDULER_H
#define SUPERZ80_DEVICES_SCHEDULER_SCHEDULER_H

#include <array>
#include <cstddef>
#include "core/types.h"

namespace sz::console {
class SuperZ80Console;
}

namespace sz::scheduler {

// Ring buffer entry for debug/troubleshooting
struct ScanlineRecord {
  u64 frame_counter_at_time = 0;
  u16 scanline = 0;
  bool vblank_flag = false;
  u32 cycles_this_scanline = 0;
  double accumulator_before = 0.0;
  double accumulator_after = 0.0;
};

struct DebugState {
  u16 current_scanline = 0;
  u64 frame_counter = 0;
  bool vblank_flag = false;
  u32 cycles_this_scanline = 0;
  double cpu_cycle_accumulator = 0.0;
  u64 total_cpu_tstates_executed = 0;
  double cpu_cycles_per_line = 0.0;
};

class Scheduler {
 public:
  static constexpr size_t kRingBufferSize = 512;

  void Reset();
  void StepOneScanline(sz::console::SuperZ80Console& console);
  void StepOneFrame(sz::console::SuperZ80Console& console);

  // Accessors
  u16 GetCurrentScanline() const { return current_scanline_; }
  u64 GetFrameCounter() const { return frame_counter_; }
  bool IsVBlank() const { return vblank_flag_; }
  u64 GetTotalCpuTStatesExecuted() const { return total_cpu_tstates_executed_; }

  DebugState GetDebugState() const;
  const std::array<ScanlineRecord, kRingBufferSize>& GetRecentAllocations() const { return ring_buffer_; }
  size_t GetRingBufferHead() const { return ring_buffer_head_; }

  // For CPU to report actual executed cycles
  void RecordCpuTStatesExecuted(u32 tstates);

 private:
  u32 ComputeCyclesThisLine();
  void RecordScanlineInRingBuffer(double acc_before, double acc_after, u32 cycles);

  // Persistent state (deterministic)
  u16 current_scanline_ = 0;        // 0-261
  u64 frame_counter_ = 0;
  u64 total_cpu_tstates_executed_ = 0;
  bool vblank_flag_ = false;
  double cpu_cycle_accumulator_ = 0.0;  // fractional remainder
  u32 cycles_this_scanline_ = 0;

  // Ring buffer for debug
  std::array<ScanlineRecord, kRingBufferSize> ring_buffer_{};
  size_t ring_buffer_head_ = 0;
};

}  // namespace sz::scheduler

#endif
