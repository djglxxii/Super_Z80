#include "devices/dma/DMAEngine.h"

#include "core/util/Assert.h"
#include "devices/bus/Bus.h"
#include "devices/ppu/PPU.h"

namespace sz::dma {

void DMAEngine::Reset() {
  dma_src_lo_ = 0;
  dma_src_hi_ = 0;
  dma_dst_lo_ = 0;
  dma_dst_hi_ = 0;
  dma_len_lo_ = 0;
  dma_len_hi_ = 0;
  dma_ctrl_ = 0;

  queued_valid_ = false;
  queued_src_ = 0;
  queued_dst_ = 0;
  queued_len_ = 0;

  last_exec_frame_ = -1;
  last_exec_scanline_ = -1;
  last_trigger_was_queued_ = false;
  last_illegal_start_ = false;
}

u8 DMAEngine::ReadReg(u8 port) {
  switch (port) {
    case kDmaPortSrcLo:
      return dma_src_lo_;
    case kDmaPortSrcHi:
      return dma_src_hi_;
    case kDmaPortDstLo:
      return dma_dst_lo_;
    case kDmaPortDstHi:
      return dma_dst_hi_;
    case kDmaPortLenLo:
      return dma_len_lo_;
    case kDmaPortLenHi:
      return dma_len_hi_;
    case kDmaPortCtrl:
      // BUSY bit is always 0 (DMA is instantaneous)
      // START bit reads as 0
      return dma_ctrl_ & kDmaCtrlQueueIfNotVBlank;
    default:
      return 0xFF;  // Unmapped ports
  }
}

void DMAEngine::WriteReg(u8 port, u8 value) {
  switch (port) {
    case kDmaPortSrcLo:
      dma_src_lo_ = value;
      break;
    case kDmaPortSrcHi:
      dma_src_hi_ = value;
      break;
    case kDmaPortDstLo:
      dma_dst_lo_ = value;
      break;
    case kDmaPortDstHi:
      dma_dst_hi_ = value;
      break;
    case kDmaPortLenLo:
      dma_len_lo_ = value;
      break;
    case kDmaPortLenHi:
      dma_len_hi_ = value;
      break;
    case kDmaPortCtrl:
      dma_ctrl_ = value;
      // Writing START bit triggers DMA logic (handled in OnScanlineBoundary)
      // Note: START trigger is edge-like, checked in OnScanlineBoundary
      break;
    default:
      // Unmapped ports: writes ignored
      break;
  }
}

void DMAEngine::OnScanlineBoundary(int scanline, bool vblank_flag, u64 frame) {
  // Phase 6: Process queued DMA at start of VBlank (scanline 192)
  if (scanline == kVBlankStartScanline && vblank_flag && queued_valid_) {
    ExecuteDMA(queued_src_, queued_dst_, queued_len_, frame, scanline);
    last_trigger_was_queued_ = true;
    queued_valid_ = false;  // Clear queue after execution
    return;
  }

  // Check if START bit is set (edge-triggered)
  if ((dma_ctrl_ & kDmaCtrlStart) == 0) {
    return;  // No START trigger
  }

  // Clear START bit (edge-triggered, auto-clear)
  dma_ctrl_ &= ~kDmaCtrlStart;

  // Compute current DMA parameters
  u16 src = static_cast<u16>((dma_src_hi_ << 8) | dma_src_lo_);
  u16 dst = static_cast<u16>((dma_dst_hi_ << 8) | dma_dst_lo_);
  u16 len = static_cast<u16>((dma_len_hi_ << 8) | dma_len_lo_);

  // Phase 6: Length rule: len == 0 => no-op
  if (len == 0) {
    return;
  }

  // Phase 6: DMA legality check
  if (vblank_flag) {
    // Legal: execute immediately
    ExecuteDMA(src, dst, len, frame, scanline);
    last_trigger_was_queued_ = false;
    last_illegal_start_ = false;
  } else {
    // Not in VBlank
    if (dma_ctrl_ & kDmaCtrlQueueIfNotVBlank) {
      // Queue the request (policy: last write wins)
      queued_valid_ = true;
      queued_src_ = src;
      queued_dst_ = dst;
      queued_len_ = len;
      last_illegal_start_ = false;
    } else {
      // Ignore the request (debug-only error flag)
      last_illegal_start_ = true;
    }
  }
}

void DMAEngine::ExecuteDMA(u16 src, u16 dst, u16 len, u64 frame, int scanline) {
  // Phase 6: Enforce VBlank-only execution (internal invariant)
  // This function should only be called when vblank_flag is true
  // (enforced by caller logic)

  if (!bus_ || !ppu_) {
    return;  // Dependencies not wired
  }

  // Phase 6: Copy data from CPU address space to VRAM
  for (u16 i = 0; i < len; ++i) {
    u8 byte = bus_->Read8(static_cast<u16>(src + i));
    ppu_->VramWriteByte(static_cast<u16>(dst + i), byte);
  }

  // Track execution
  last_exec_frame_ = static_cast<int>(frame);
  last_exec_scanline_ = scanline;

  // Optional: Raise DMA_DONE IRQ (not required for Phase 6 pass)
  // if (irq_) {
  //   irq_->Raise(static_cast<u8>(sz::irq::IrqBit::DmaDone));
  // }
}

DebugState DMAEngine::GetDebugState() const {
  DebugState state;

  // Current DMA registers
  state.src = static_cast<u16>((dma_src_hi_ << 8) | dma_src_lo_);
  state.dst = static_cast<u16>((dma_dst_hi_ << 8) | dma_dst_lo_);
  state.len = static_cast<u16>((dma_len_hi_ << 8) | dma_len_lo_);
  state.ctrl = dma_ctrl_;
  state.queue_enabled = (dma_ctrl_ & kDmaCtrlQueueIfNotVBlank) != 0;

  // Queued DMA state
  state.queued_valid = queued_valid_;
  state.queued_src = queued_src_;
  state.queued_dst = queued_dst_;
  state.queued_len = queued_len_;

  // Last execution tracking
  state.last_exec_frame = last_exec_frame_;
  state.last_exec_scanline = last_exec_scanline_;
  state.last_trigger_was_queued = last_trigger_was_queued_;

  // Error tracking
  state.last_illegal_start = last_illegal_start_;

  return state;
}

}  // namespace sz::dma
