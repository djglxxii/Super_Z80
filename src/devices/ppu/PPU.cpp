#include "devices/ppu/PPU.h"

namespace sz::ppu {

void PPU::Reset() {
  last_scanline_ = -1;
  vblank_flag_ = false;
  last_vblank_latch_scanline_ = 0xFFFF;
  vblank_latch_count_ = 0;

  // Phase 6: Initialize VRAM (all zeros)
  vram_.assign(kVramSizeBytes, 0x00);
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

// Phase 6: VRAM write APIs (address wraps modulo kVramSizeBytes)
void PPU::VramWriteByte(u16 addr, u8 value) {
  size_t effective = static_cast<size_t>(addr) % kVramSizeBytes;
  vram_[effective] = value;
}

u8 PPU::VramReadByte(u16 addr) const {
  size_t effective = static_cast<size_t>(addr) % kVramSizeBytes;
  return vram_[effective];
}

void PPU::VramWriteBlock(u16 dst, std::span<const u8> bytes) {
  for (size_t i = 0; i < bytes.size(); ++i) {
    size_t effective = (static_cast<size_t>(dst) + i) % kVramSizeBytes;
    vram_[effective] = bytes[i];
  }
}

std::vector<u8> PPU::VramReadWindow(u16 start, size_t count) const {
  std::vector<u8> result;
  result.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    size_t effective = (static_cast<size_t>(start) + i) % kVramSizeBytes;
    result.push_back(vram_[effective]);
  }
  return result;
}

}  // namespace sz::ppu
