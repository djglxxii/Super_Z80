#ifndef SUPERZ80_CPU_Z80CPU_H
#define SUPERZ80_CPU_Z80CPU_H

#include "core/types.h"

#include <cstdint>

namespace sz::bus {
class Bus;
}

namespace sz::cpu {

// Z80 register set for debug state
struct Z80Regs {
  u16 pc, sp;
  u16 af, bc, de, hl;
  u16 af2, bc2, de2, hl2;
  u16 ix, iy;
  u8 i, r;
  bool iff1, iff2;
  u8 im;
};

// Last instruction executed
struct Z80LastInsn {
  u16 pc;
  u8 bytes[4];
  u8 len;
  u8 tstates;
};

// Full debug state
struct DebugState {
  Z80Regs regs{};
  Z80LastInsn last{};
  u64 total_tstates = 0;
  bool int_line = false;
};

class Z80Cpu {
 public:
  explicit Z80Cpu(sz::bus::Bus& bus);
  ~Z80Cpu();

  // Non-copyable
  Z80Cpu(const Z80Cpu&) = delete;
  Z80Cpu& operator=(const Z80Cpu&) = delete;

  void Reset();
  u32 Step(u32 tstates_budget);  // returns actual T-states executed
  DebugState GetDebugState() const;

  // Set external /INT line (level-sensitive)
  void SetIntLine(bool asserted);

 private:
  friend struct Z80CpuCallbacks;

  sz::bus::Bus& bus_;
  void* cpu_;  // z80ex context (opaque to avoid header pollution)

  DebugState dbg_{};
  bool int_line_ = false;

  // Instruction byte capture for debug
  bool capture_active_ = false;
  u16 capture_expected_addr_ = 0;
  u8 capture_len_ = 0;

  void RefreshDebugRegs();
};

}  // namespace sz::cpu

#endif
