#include "devices/ppu/PPU.h"

namespace sz::ppu {

void PPU::Reset() {
  last_scanline_ = -1;
}

void PPU::RenderScanline(int scanline, Framebuffer& /*fb*/) {
  last_scanline_ = scanline;
}

DebugState PPU::GetDebugState() const {
  DebugState state;
  state.last_scanline = last_scanline_;
  return state;
}

}  // namespace sz::ppu
