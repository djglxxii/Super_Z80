#ifndef SUPERZ80_DEVICES_DMA_DMAENGINE_H
#define SUPERZ80_DEVICES_DMA_DMAENGINE_H

#include "core/types.h"

namespace sz::bus {
class Bus;
}

namespace sz::ppu {
class PPU;
}

namespace sz::irq {
class IRQController;
}

namespace sz::dma {

// Phase 6: DMA_CTRL register bit definitions
constexpr u8 kDmaCtrlStart = 0x01;              // bit 0: START trigger
constexpr u8 kDmaCtrlQueueIfNotVBlank = 0x02;   // bit 1: QUEUE_IF_NOT_VBLANK
constexpr u8 kDmaCtrlDstIsPalette = 0x08;       // bit 3: DST_IS_PALETTE (Phase 8)
constexpr u8 kDmaCtrlBusy = 0x80;               // bit 7: BUSY (read-only)

// Phase 6: DMA I/O port addresses
constexpr u8 kDmaPortSrcLo = 0x30;
constexpr u8 kDmaPortSrcHi = 0x31;
constexpr u8 kDmaPortDstLo = 0x32;
constexpr u8 kDmaPortDstHi = 0x33;
constexpr u8 kDmaPortLenLo = 0x34;
constexpr u8 kDmaPortLenHi = 0x35;
constexpr u8 kDmaPortCtrl = 0x36;

struct DebugState {
  // Current DMA registers
  u16 src = 0;
  u16 dst = 0;
  u16 len = 0;
  u8 ctrl = 0;
  bool queue_enabled = false;
  bool dst_is_palette = false;  // Phase 8: Palette destination flag

  // Queued DMA state
  bool queued_valid = false;
  u16 queued_src = 0;
  u16 queued_dst = 0;
  u16 queued_len = 0;
  bool queued_dst_is_palette = false;  // Phase 8: Queued palette flag

  // Last execution tracking
  int last_exec_frame = -1;
  int last_exec_scanline = -1;
  bool last_trigger_was_queued = false;
  bool last_exec_was_palette = false;  // Phase 8: Last exec was palette DMA

  // Error tracking (debug only)
  bool last_illegal_start = false;
};

class DMAEngine {
 public:
  // Constructor with dependency injection
  DMAEngine() = default;

  void Reset();

  // Phase 6: I/O register access (called from Bus)
  u8 ReadReg(u8 port);
  void WriteReg(u8 port, u8 value);

  // Phase 6: Called once per scanline from console/scheduler loop
  void OnScanlineBoundary(int scanline, bool vblank_flag, u64 frame);

  // Phase 6: Wire dependencies
  void SetBus(sz::bus::Bus* bus) { bus_ = bus; }
  void SetPPU(sz::ppu::PPU* ppu) { ppu_ = ppu; }
  void SetIRQController(sz::irq::IRQController* irq) { irq_ = irq; }

  DebugState GetDebugState() const;

 private:
  // Execute DMA transfer (only when vblank_flag is true)
  // Phase 8: dst_is_palette selects VRAM vs Palette RAM destination
  void ExecuteDMA(u16 src, u16 dst, u16 len, bool dst_is_palette, u64 frame, int scanline);

  // DMA registers (R/W)
  u8 dma_src_lo_ = 0;
  u8 dma_src_hi_ = 0;
  u8 dma_dst_lo_ = 0;
  u8 dma_dst_hi_ = 0;
  u8 dma_len_lo_ = 0;
  u8 dma_len_hi_ = 0;
  u8 dma_ctrl_ = 0;

  // Queued DMA request (policy: last write wins)
  bool queued_valid_ = false;
  u16 queued_src_ = 0;
  u16 queued_dst_ = 0;
  u16 queued_len_ = 0;
  bool queued_dst_is_palette_ = false;  // Phase 8: Queued palette flag

  // Last execution tracking
  int last_exec_frame_ = -1;
  int last_exec_scanline_ = -1;
  bool last_trigger_was_queued_ = false;
  bool last_exec_was_palette_ = false;  // Phase 8: Last exec was palette DMA

  // Debug-only error flag
  bool last_illegal_start_ = false;

  // Dependencies
  sz::bus::Bus* bus_ = nullptr;
  sz::ppu::PPU* ppu_ = nullptr;
  sz::irq::IRQController* irq_ = nullptr;
};

}  // namespace sz::dma

#endif
