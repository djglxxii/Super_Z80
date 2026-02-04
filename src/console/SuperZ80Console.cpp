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
  cartridge_.Reset();
  ppu_.Reset();
  apu_.Reset();
  dma_.Reset();
  input_.Reset();
  cpu_.Reset();
}

void SuperZ80Console::StepFrame() {
  // Phase 3: Use Scheduler::StepOneFrame which internally calls StepOneScanline
  // exactly 262 times with correct execution order
  scheduler_.StepOneFrame(*this);
}

// Scheduler hook implementations (called by Scheduler::StepOneScanline)
void SuperZ80Console::ExecuteCpu(u32 tstates) {
  u32 executed = cpu_.Step(tstates);
  scheduler_.RecordCpuTStatesExecuted(executed);
}

void SuperZ80Console::TickIRQ() {
  // Phase 3: IRQ must remain deasserted (no sources)
  irq_.Tick();
}

void SuperZ80Console::OnVisibleScanline(u16 scanline) {
  // Phase 3: PPU stub - maintain Phase 0 test pattern path
  ppu_.RenderScanline(scanline, framebuffer_);
}

void SuperZ80Console::TickDMA() {
  // Phase 3: DMA stub (no-op)
  dma_.Tick();
}

void SuperZ80Console::TickAPU(u32 cycles) {
  // Phase 3: APU stub (no-op)
  apu_.Tick(cycles);
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
  state.scanline = sched.current_scanline;
  state.frame = sched.frame_counter;
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
