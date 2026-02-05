#include "devices/ppu/PPU.h"

namespace sz::ppu {

void PPU::Reset() {
  last_scanline_ = -1;
  vblank_flag_ = false;
  last_vblank_latch_scanline_ = 0xFFFF;
  vblank_latch_count_ = 0;
}

void PPU::RenderScanline(int scanline, Framebuffer& /*fb*/) {
  last_scanline_ = scanline;
}

void PPU::OnScanlineStart(u16 scanline) {
  // Phase 5: VBlank flag timing (exact per timing model)
  // vblank_flag = true at START of scanline 192
  // vblank_flag = false at START of scanline 0
  if (scanline == kVBlankStartScanline) {
    vblank_flag_ = true;
    last_vblank_latch_scanline_ = scanline;
    vblank_latch_count_++;
  } else if (scanline == 0) {
    vblank_flag_ = false;
  }
}

DebugState PPU::GetDebugState() const {
  DebugState state;
  state.last_scanline = last_scanline_;
  state.vblank_flag = vblank_flag_;
  state.last_vblank_latch_scanline = last_vblank_latch_scanline_;
  state.vblank_latch_count = vblank_latch_count_;
  return state;
}

}  // namespace sz::ppu
