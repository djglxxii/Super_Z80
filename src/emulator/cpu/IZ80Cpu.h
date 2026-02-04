#pragma once
#include <cstdint>

struct Z80Regs {
  uint16_t pc, sp;
  uint16_t af, bc, de, hl;
  uint16_t af2, bc2, de2, hl2;
  uint16_t ix, iy;
  uint8_t i, r;
  bool iff1, iff2;
  uint8_t im;
};

struct Z80LastInsn {
  uint16_t pc;
  uint8_t bytes[4];
  uint8_t len;
  uint8_t tstates;
};

struct Z80DebugState {
  Z80Regs regs;
  Z80LastInsn last;
  uint64_t total_tstates;
};

class IBus;

class IZ80Cpu {
public:
  virtual ~IZ80Cpu() = default;

  virtual void Reset() = 0;
  virtual uint32_t RunTStates(uint32_t tstate_budget) = 0;
  virtual void SetIntLine(bool asserted) = 0;
  virtual Z80DebugState GetDebugState() const = 0;
};
