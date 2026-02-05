#ifndef SUPERZ80_DEVICES_PPU_PPU_H
#define SUPERZ80_DEVICES_PPU_PPU_H

#include <array>
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

// Phase 7: PPU register set (active and pending copies)
struct PpuRegs {
  u8 vdp_ctrl = 0;       // 0x11: bit 0 = display enable
  u8 scroll_x = 0;       // 0x12: Plane A fine scroll X (pixels)
  u8 scroll_y = 0;       // 0x13: Plane A fine scroll Y (pixels)
  u8 plane_a_base = 0;   // 0x16: Tilemap base selector (VRAM page index, *1024)
  u8 pattern_base = 0;   // 0x18: Pattern base selector (VRAM page index, *1024)
};

struct DebugState {
  int last_scanline = -1;
  bool vblank_flag = false;
  u16 last_vblank_latch_scanline = 0xFFFF;
  u64 vblank_latch_count = 0;
  // Phase 7: Register visibility
  PpuRegs active_regs;
  PpuRegs pending_regs;
};

class PPU {
 public:
  // Phase 7: VRAM size (48KB per spec)
  static constexpr size_t kVramSizeBytes = 49152;  // 48KB

  // Phase 7: Fixed palette (16 colors for Phase 7, 9-bit RGB packed as ARGB8888)
  static constexpr size_t kPaletteSize = 16;

  void Reset();

  // Phase 7: I/O port handlers (all writes go to pending_regs)
  u8 IoRead(u8 port);
  void IoWrite(u8 port, u8 value);

  // Phase 7: Scanline rendering interface
  void BeginScanline(int scanline);  // Latch pending→active, update VBlank flag
  void RenderScanline(int scanline, Framebuffer& fb);

  // Phase 5: Read VBlank flag for VDP_STATUS port
  bool GetVBlankFlag() const { return vblank_flag_; }

  // Phase 6: VRAM write APIs for DMA (destination wraps modulo kVramSizeBytes)
  void VramWriteByte(u16 addr, u8 value);
  u8 VramReadByte(u16 addr) const;
  void VramWriteBlock(u16 dst, std::span<const u8> bytes);

  // Phase 6: VRAM debug window read
  std::vector<u8> VramReadWindow(u16 start, size_t count) const;

  // Phase 7: Debug access to registers
  const PpuRegs& GetActiveRegs() const { return active_regs_; }
  const PpuRegs& GetPendingRegs() const { return pending_regs_; }

  DebugState GetDebugState() const;

 private:
  // Phase 7: Tile decode (packed 4bpp)
  // Returns palette index (0-15) for pixel (x, y) within tile
  u8 DecodeTilePixel(u16 tile_index, int x_in_tile, int y_in_tile) const;

  // Phase 7: Tilemap entry fetch (16-bit little-endian)
  // Returns tile index (bits 0-9); ignores flip/priority for Phase 7
  u16 FetchTilemapEntry(int tile_x, int tile_y) const;

  // Phase 7: Map palette index to ARGB8888 color
  u32 PaletteToArgb(u8 palette_index) const;

  // Phase 7: Initialize test pattern in VRAM for startup display
  void InitTestPattern();

  int last_scanline_ = -1;

  // Phase 5: VBlank state
  bool vblank_flag_ = false;
  u16 last_vblank_latch_scanline_ = 0xFFFF;
  u64 vblank_latch_count_ = 0;

  // Phase 6: VRAM backing store
  std::vector<u8> vram_;

  // Phase 7: Register sets (active latched at scanline start, pending written by CPU)
  PpuRegs active_regs_;
  PpuRegs pending_regs_;

  // Phase 7: Fixed palette (ARGB8888 format)
  std::array<u32, kPaletteSize> palette_;
};

}  // namespace sz::ppu

#endif
