#include "devices/irq/IRQController.h"
#include "core/util/Assert.h"
#include "core/log/Logger.h"

namespace sz::irq {

void IRQController::Reset() {
  pending_ = 0;
  enable_ = 0;
  int_line_ = false;
  isr_entry_count_ = 0;
  synthetic_fire_count_ = 0;
}

void IRQController::Raise(u8 pending_mask) {
  pending_ |= pending_mask;
  // Note: /INT is recomputed in PreCpuUpdate/PostCpuUpdate, not here
}

void IRQController::Ack(u8 w1c_mask) {
  // Write-1-to-clear: for each bit that is 1 in w1c_mask, clear that pending bit
  pending_ &= ~w1c_mask;

  // Recompute /INT immediately to satisfy "drops immediately (same scanline step)"
  RecomputeIntLine();

  // Hard assertion #2: Immediate drop
  SZ_ASSERT(int_line_ == ((pending_ & enable_) != 0));
}

u8 IRQController::ReadEnable() const {
  return enable_;
}

void IRQController::WriteEnable(u8 mask) {
  enable_ = mask;
  // Recompute /INT immediately
  RecomputeIntLine();
}

u8 IRQController::ReadStatus() const {
  // Return latched pending bits - no auto-clear on read
  return pending_;
}

void IRQController::PreCpuUpdate() {
  RecomputeIntLine();

  // Hard assertion #1: Masking works
  if (pending_ != 0 && (pending_ & enable_) == 0) {
    SZ_ASSERT(int_line_ == false);
  }
}

void IRQController::PostCpuUpdate() {
  RecomputeIntLine();

  // Hard assertion #1: Masking works
  if (pending_ != 0 && (pending_ & enable_) == 0) {
    SZ_ASSERT(int_line_ == false);
  }
}

bool IRQController::IntLineAsserted() const {
  return int_line_;
}

DebugState IRQController::GetDebug(u16 current_scanline) const {
  DebugState state;
  state.scanline = current_scanline;
  state.pending = pending_;
  state.enable = enable_;
  state.int_line_asserted = int_line_;
  state.isr_entry_count = isr_entry_count_;
  state.synthetic_fire_count = synthetic_fire_count_;
  return state;
}

void IRQController::RecomputeIntLine() {
  int_line_ = ((pending_ & enable_) != 0);
}

}  // namespace sz::irq
