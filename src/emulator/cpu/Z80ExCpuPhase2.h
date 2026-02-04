#pragma once
#include "emulator/cpu/IZ80Cpu.h"

class Phase2Bus;

// Phase 2 Z80 CPU adapter: routes I/O through Phase2Bus
class Z80ExCpuPhase2 final : public IZ80Cpu {
public:
  explicit Z80ExCpuPhase2(Phase2Bus& bus);
  ~Z80ExCpuPhase2() override;

  void Reset() override;
  uint32_t RunTStates(uint32_t tstate_budget) override;
  void SetIntLine(bool asserted) override;
  Z80DebugState GetDebugState() const override;

private:
  friend struct Z80ExPhase2Callbacks;

  Phase2Bus& bus_;
  void* cpu_;

  Z80DebugState dbg_{};
  bool int_line_ = false;

  bool capture_active_ = false;
  uint16_t capture_expected_addr_ = 0;
  uint8_t capture_len_ = 0;

  void RefreshDebugRegs();
};
