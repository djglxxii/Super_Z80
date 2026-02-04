#ifndef SUPERZ80_DEVICES_IRQ_IRQCONTROLLER_H
#define SUPERZ80_DEVICES_IRQ_IRQCONTROLLER_H

#include <cstdint>
#include "core/types.h"

namespace sz::irq {

// IRQ source bit definitions
enum class IrqBit : u8 {
  VBlank      = 1 << 0,
  Timer       = 1 << 1,
  Scanline    = 1 << 2,
  SprOverflow = 1 << 3,
  DmaDone     = 1 << 4,  // optional; keep defined but unused in Phase 4
};

struct DebugState {
  u16 scanline = 0;                  // provided by Scheduler for display
  u8 pending = 0;                    // latched
  u8 enable = 0;                     // mask
  bool int_line_asserted = false;    // derived
  u64 isr_entry_count = 0;           // maintained by test harness observation
  u64 synthetic_fire_count = 0;      // maintained by scheduler trigger
};

class IRQController {
 public:
  void Reset();  // clears pending, enable=0, /INT deasserted

  // Latch/set pending bits (OR-in), regardless of enable mask.
  void Raise(u8 pending_mask);

  // Write-1-to-clear latched pending bits.
  void Ack(u8 w1c_mask);

  // R/W enable
  u8 ReadEnable() const;
  void WriteEnable(u8 mask);

  // R-only status (no side effects)
  u8 ReadStatus() const;

  // Called at scanline start AFTER any scanline-start synthetic raises.
  void PreCpuUpdate();   // recompute /INT (level) from pending & enable

  // Called after CPU ran this scanline's cycles and after any I/O writes occurred.
  void PostCpuUpdate();  // recompute /INT again so ACK drops immediately

  bool IntLineAsserted() const;

  DebugState GetDebug(u16 current_scanline) const;

  // For test harness observation
  void IncrementIsrEntryCount() { isr_entry_count_++; }
  void IncrementSyntheticFireCount() { synthetic_fire_count_++; }

 private:
  u8 pending_ = 0;
  u8 enable_  = 0;
  bool int_line_ = false;
  u64 isr_entry_count_ = 0;
  u64 synthetic_fire_count_ = 0;

  void RecomputeIntLine();  // int_line_ = ((pending_ & enable_) != 0)
};

}  // namespace sz::irq

#endif
