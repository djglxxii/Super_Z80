#include "console/SuperZ80Console.h"

#include "core/log/Logger.h"
#include "core/types.h"
#include "core/util/Assert.h"

namespace sz::console {

SuperZ80Console::SuperZ80Console() = default;

SuperZ80Console::~SuperZ80Console() = default;

bool SuperZ80Console::PowerOn() {
  framebuffer_.width = kScreenWidth;
  framebuffer_.height = kScreenHeight;
  framebuffer_.pixels.assign(static_cast<size_t>(kScreenWidth * kScreenHeight), 0xFF000000u);
  SZ_ASSERT(static_cast<int>(framebuffer_.pixels.size()) == kScreenWidth * kScreenHeight);
  SZ_LOG_INFO("SuperZ80Console PowerOn: framebuffer %dx%d", framebuffer_.width, framebuffer_.height);

  // Wire IRQController to Bus for I/O port access
  bus_.SetIRQController(&irq_);

  // Phase 5: Wire PPU to Bus for VDP_STATUS port access
  bus_.SetPPU(&ppu_);

  // Phase 6: Wire DMAEngine dependencies
  dma_.SetBus(&bus_);
  dma_.SetPPU(&ppu_);
  dma_.SetIRQController(&irq_);

  // Phase 6: Wire DMAEngine to Bus for DMA I/O port access
  bus_.SetDMAEngine(&dma_);

  // Phase 9: Create real Z80 CPU (must be after bus_ is set up)
  cpu_ = std::make_unique<sz::cpu::Z80Cpu>(bus_);

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
  if (cpu_) {
    cpu_->Reset();
  }
}

bool SuperZ80Console::LoadRom(const char* path) {
  if (!bus_.LoadRomFromFile(path)) {
    return false;
  }
  SZ_LOG_INFO("SuperZ80Console: ROM loaded from '%s'", path);
  return true;
}

bool SuperZ80Console::IsRomLoaded() const {
  return bus_.IsRomLoaded();
}

void SuperZ80Console::StepFrame() {
  // Phase 3: Use Scheduler::StepOneFrame which internally calls StepOneScanline
  // exactly 262 times with correct execution order
  scheduler_.StepOneFrame(*this);
}

// Scheduler hook implementations (called by Scheduler::StepOneScanline)
void SuperZ80Console::OnScanlineStart(u16 scanline) {
  // Phase 8: Update PPU's current frame for debug tracking
  ppu_.SetCurrentFrame(scheduler_.GetFrameCounter());

  // Phase 7: Call PPU's BeginScanline to latch registers and handle VBlank flag
  ppu_.BeginScanline(static_cast<int>(scanline));

  // Phase 9: Update IRQController's current scanline for VBlank tracking
  irq_.SetCurrentScanline(scanline);

  // Phase 5: Replace synthetic IRQ with real VBlank IRQ at scanline 192
  if (scanline == kVBlankStartScanline) {
    irq_.Raise(static_cast<u8>(sz::irq::IrqBit::VBlank));
  }

  // Recompute /INT before CPU runs this scanline
  irq_.PreCpuUpdate();

  // Drive CPU's /INT input (level-sensitive)
  if (cpu_) {
    cpu_->SetIntLine(irq_.IntLineAsserted());
  }
}

void SuperZ80Console::ExecuteCpu(u32 tstates) {
  if (cpu_) {
    u32 executed = cpu_->Step(tstates);
    scheduler_.RecordCpuTStatesExecuted(executed);
  }
}

void SuperZ80Console::TickIRQ() {
  // Phase 4: PostCpuUpdate to ensure ACK clears drop /INT immediately
  irq_.PostCpuUpdate();

  // Update CPU's /INT input after post-CPU update
  if (cpu_) {
    cpu_->SetIntLine(irq_.IntLineAsserted());
  }
}

void SuperZ80Console::OnVisibleScanline(u16 scanline) {
  // Phase 3: PPU stub - maintain Phase 0 test pattern path
  ppu_.RenderScanline(scanline, framebuffer_);
}

void SuperZ80Console::TickDMA() {
  // Phase 6: DMA processes queued requests during VBlank
  int scanline = static_cast<int>(scheduler_.GetCurrentScanline());
  bool vblank = ppu_.GetVBlankFlag();
  u64 frame = scheduler_.GetFrameCounter();
  dma_.OnScanlineBoundary(scanline, vblank, frame);
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
  return irq_.GetDebug(scheduler_.GetCurrentScanline());
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
  if (cpu_) {
    return cpu_->GetDebugState();
  }
  return {};
}

std::vector<u8> SuperZ80Console::GetPPUVramWindow(u16 start, size_t count) const {
  return ppu_.VramReadWindow(start, count);
}

}  // namespace sz::console
