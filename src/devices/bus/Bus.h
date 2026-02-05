#ifndef SUPERZ80_DEVICES_BUS_BUS_H
#define SUPERZ80_DEVICES_BUS_BUS_H

#include "core/types.h"

namespace sz::irq {
class IRQController;
}

namespace sz::ppu {
class PPU;
}

namespace sz::bus {

struct DebugState {
  bool placeholder = true;
};

class Bus {
 public:
  void Reset();
  u8 Read8(u16 addr);
  void Write8(u16 addr, u8 value);
  u8 In8(u8 port);
  void Out8(u8 port, u8 value);
  DebugState GetDebugState() const;

  // Wire IRQController for I/O port access
  void SetIRQController(sz::irq::IRQController* irq) { irq_ = irq; }

  // Phase 5: Wire PPU for VDP_STATUS port access
  void SetPPU(sz::ppu::PPU* ppu) { ppu_ = ppu; }

 private:
  sz::irq::IRQController* irq_ = nullptr;
  sz::ppu::PPU* ppu_ = nullptr;
};

}  // namespace sz::bus

#endif
