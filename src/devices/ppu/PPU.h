#ifndef SUPERZ80_DEVICES_PPU_PPU_H
#define SUPERZ80_DEVICES_PPU_PPU_H

#include <cstddef>
#include <span>
#include <vector>

#include "core/types.h"

namespace sz::ppu {

struct Framebuffer {
  std::vector<u32> pixels;
  int width = kScreenWidth;
  int height = kScreenHeight;
};

struct DebugState {
  int last_scanline = -1;
  bool vblank_flag = false;
  u16 last_vblank_latch_scanline = 0xFFFF;
  u64 vblank_latch_count = 0;
};

class PPU {
 public:
  // Phase 6: VRAM size (placeholder, revise if spec clarifies)
  static constexpr size_t kVramSizeBytes = 16384;  // 16KB placeholder

  void Reset();
  void RenderScanline(int scanline, Framebuffer& fb);

  // Phase 5: VBlank timing hooks (called by Console from Scheduler boundary)
  void OnScanlineStart(u16 scanline);

  // Phase 5: Read VBlank flag for VDP_STATUS port
  bool GetVBlankFlag() const { return vblank_flag_; }

  // Phase 6: VRAM write APIs for DMA (destination wraps modulo kVramSizeBytes)
  void VramWriteByte(u16 addr, u8 value);
  u8 VramReadByte(u16 addr) const;
  void VramWriteBlock(u16 dst, std::span<const u8> bytes);

  // Phase 6: VRAM debug window read
  std::vector<u8> VramReadWindow(u16 start, size_t count) const;

  DebugState GetDebugState() const;

 private:
  int last_scanline_ = -1;

  // Phase 5: VBlank state (no rendering changes)
  bool vblank_flag_ = false;
  u16 last_vblank_latch_scanline_ = 0xFFFF;
  u64 vblank_latch_count_ = 0;

  // Phase 6: VRAM backing store
  std::vector<u8> vram_;
};

}  // namespace sz::ppu

#endif
