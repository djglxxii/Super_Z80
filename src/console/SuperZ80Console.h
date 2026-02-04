#ifndef SUPERZ80_CONSOLE_SUPERZ80CONSOLE_H
#define SUPERZ80_CONSOLE_SUPERZ80CONSOLE_H

#include "cpu/Z80CpuStub.h"
#include "devices/apu/APU.h"
#include "devices/bus/Bus.h"
#include "devices/cart/Cartridge.h"
#include "devices/dma/DMAEngine.h"
#include "devices/io/IODevice.h"
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
  bool PowerOn();
  void Reset();
  void StepFrame();
  const sz::ppu::Framebuffer& GetFramebuffer() const;
  sz::ppu::Framebuffer& GetFramebufferMutable();
  DebugState GetDebugState() const;

  void SetHostButtons(const sz::input::HostButtons& buttons);

  sz::scheduler::DebugState GetSchedulerDebugState() const;
  sz::bus::DebugState GetBusDebugState() const;
  sz::irq::DebugState GetIRQDebugState() const;
  sz::ppu::DebugState GetPPUDebugState() const;
  sz::apu::DebugState GetAPUDebugState() const;
  sz::dma::DebugState GetDMADebugState() const;
  sz::cart::DebugState GetCartridgeDebugState() const;
  sz::input::DebugState GetInputDebugState() const;
  sz::cpu::DebugState GetCpuDebugState() const;

 private:
 sz::scheduler::Scheduler scheduler_{};
  sz::irq::IRQController irq_{};
  sz::cart::Cartridge cartridge_{};
  sz::io::IODevice io_device_{};
  sz::bus::Bus bus_{cartridge_, io_device_};
  sz::ppu::PPU ppu_{};
  sz::apu::APU apu_{};
  sz::dma::DMAEngine dma_{};
  sz::input::InputController input_{};
  sz::cpu::Z80CpuStub cpu_{};

  sz::ppu::Framebuffer framebuffer_{};
};

}  // namespace sz::console

#endif
