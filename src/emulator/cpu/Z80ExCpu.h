#pragma once
#include "emulator/cpu/IZ80Cpu.h"

class IBus;

class Z80ExCpu final : public IZ80Cpu {
public:
  explicit Z80ExCpu(IBus& bus);
  ~Z80ExCpu() override;

  void Reset() override;
  uint32_t RunTStates(uint32_t tstate_budget) override;
  void SetIntLine(bool asserted) override;
  Z80DebugState GetDebugState() const override;

private:
  friend struct Z80ExCallbacks;

  IBus& bus_;
  void* cpu_;

  Z80DebugState dbg_{};
  bool int_line_ = false;

  bool capture_active_ = false;
  uint16_t capture_expected_addr_ = 0;
  uint8_t capture_len_ = 0;

  void RefreshDebugRegs();
};
