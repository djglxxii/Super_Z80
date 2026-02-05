#ifndef SUPERZ80_CONSOLE_SUPERZ80CONSOLE_H
#define SUPERZ80_CONSOLE_SUPERZ80CONSOLE_H

#include <memory>

#include "cpu/Z80Cpu.h"
#include "devices/apu/APU.h"
#include "devices/bus/Bus.h"
#include "devices/cart/Cartridge.h"
#include "devices/dma/DMAEngine.h"
#include "devices/input/InputController.h"
#include "devices/irq/IRQController.h"
#include "devices/ppu/PPU.h"
#include "devices/scheduler/Scheduler.h"

namespace sz::console {

struct DebugState {
  int scanline = 0;
  u64 frame = 0;
};

class SuperZ80Console {
 public:
  SuperZ80Console();
  ~SuperZ80Console();

  bool PowerOn();
  void Reset();
  void StepFrame();
  const sz::ppu::Framebuffer& GetFramebuffer() const;
  sz::ppu::Framebuffer& GetFramebufferMutable();
  DebugState GetDebugState() const;

  void SetHostButtons(const sz::input::HostButtons& buttons);

  // Phase 9: ROM loading
  bool LoadRom(const char* path);
  bool IsRomLoaded() const;

  // Debug state accessors
  sz::scheduler::DebugState GetSchedulerDebugState() const;
  sz::bus::DebugState GetBusDebugState() const;
  sz::irq::DebugState GetIRQDebugState() const;
  sz::ppu::DebugState GetPPUDebugState() const;
  sz::apu::DebugState GetAPUDebugState() const;
  sz::dma::DebugState GetDMADebugState() const;
  sz::cart::DebugState GetCartridgeDebugState() const;
  sz::input::DebugState GetInputDebugState() const;
  sz::cpu::DebugState GetCpuDebugState() const;

  // Phase 7: PPU debug access
  std::vector<u8> GetPPUVramWindow(u16 start, size_t count) const;
  const sz::ppu::PPU& GetPPU() const { return ppu_; }

  // Phase 9: Direct access to Bus for debug
  const sz::bus::Bus& GetBus() const { return bus_; }

  // Scheduler hooks (called by Scheduler::StepOneScanline)
  void OnScanlineStart(u16 scanline);
  void ExecuteCpu(u32 tstates);
  void TickIRQ();
  void OnVisibleScanline(u16 scanline);
  void TickDMA();
  void TickAPU(u32 cycles);

 private:
  sz::scheduler::Scheduler scheduler_{};
  sz::bus::Bus bus_{};
  sz::irq::IRQController irq_{};
  sz::cart::Cartridge cartridge_{};
  sz::ppu::PPU ppu_{};
  sz::apu::APU apu_{};
  sz::dma::DMAEngine dma_{};
  sz::input::InputController input_{};

  // Phase 9: Real Z80 CPU (uses z80ex)
  // Must be after bus_ since it takes a reference to bus_
  std::unique_ptr<sz::cpu::Z80Cpu> cpu_;

  sz::ppu::Framebuffer framebuffer_{};
};

}  // namespace sz::console

#endif
