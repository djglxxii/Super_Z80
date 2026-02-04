#include "console/SuperZ80Console.h"

#include "core/log/Logger.h"
#include "core/types.h"
#include "core/util/Assert.h"

namespace sz::console {

bool SuperZ80Console::PowerOn() {
  framebuffer_.width = kScreenWidth;
  framebuffer_.height = kScreenHeight;
  framebuffer_.pixels.assign(static_cast<size_t>(kScreenWidth * kScreenHeight), 0xFF000000u);
  SZ_ASSERT(static_cast<int>(framebuffer_.pixels.size()) == kScreenWidth * kScreenHeight);
  SZ_LOG_INFO("SuperZ80Console PowerOn: framebuffer %dx%d", framebuffer_.width, framebuffer_.height);
  return true;
}

void SuperZ80Console::Reset() {
  scheduler_.Reset();
  bus_.Reset();
  irq_.Reset();
  ppu_.Reset();
  apu_.Reset();
  dma_.Reset();
  input_.Reset();
  cpu_.Reset();
}

void SuperZ80Console::StepFrame() {
  scheduler_.BeginFrame();

  for (int scanline = 0; scanline < kTotalScanlines; ++scanline) {
    int cpu_budget = scheduler_.ComputeCpuBudgetTstatesForScanline();
    cpu_.Step(cpu_budget);
    irq_.Tick();
    ppu_.RenderScanline(scanline, framebuffer_);
    if (scanline == kVBlankStartScanline) {
      // VBlank boundary hook placeholder.
    }
    dma_.Tick();
    apu_.Tick(cpu_budget);
    scheduler_.StepScanline();
  }

  scheduler_.EndFrame();
}

const sz::ppu::Framebuffer& SuperZ80Console::GetFramebuffer() const {
  return framebuffer_;
}

sz::ppu::Framebuffer& SuperZ80Console::GetFramebufferMutable() {
  return framebuffer_;
}

DebugState SuperZ80Console::GetDebugState() const {
  DebugState state;
  auto sched = scheduler_.GetDebugState();
  state.scanline = sched.scanline;
  state.frame = sched.frame;
  return state;
}

void SuperZ80Console::SetHostButtons(const sz::input::HostButtons& buttons) {
  input_.SetHostButtons(buttons);
}

sz::scheduler::DebugState SuperZ80Console::GetSchedulerDebugState() const {
  return scheduler_.GetDebugState();
}

sz::bus::DebugState SuperZ80Console::GetBusDebugState() const {
  return bus_.GetDebugState();
}

sz::irq::DebugState SuperZ80Console::GetIRQDebugState() const {
  return irq_.GetDebugState();
}

sz::ppu::DebugState SuperZ80Console::GetPPUDebugState() const {
  return ppu_.GetDebugState();
}

sz::apu::DebugState SuperZ80Console::GetAPUDebugState() const {
  return apu_.GetDebugState();
}

sz::dma::DebugState SuperZ80Console::GetDMADebugState() const {
  return dma_.GetDebugState();
}

sz::cart::DebugState SuperZ80Console::GetCartridgeDebugState() const {
  return cartridge_.GetDebugState();
}

sz::input::DebugState SuperZ80Console::GetInputDebugState() const {
  return input_.GetDebugState();
}

sz::cpu::DebugState SuperZ80Console::GetCpuDebugState() const {
  return cpu_.GetDebugState();
}

}  // namespace sz::console
