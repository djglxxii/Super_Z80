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

// Phase 7/10: PPU register set (active and pending copies)
// VDP_CTRL bit layout:
//   bit 0 = display enable (global)
//   bit 1 = Plane B enable (Phase 10)
// Plane A is implicitly enabled when display is enabled (backward compatible)
struct PpuRegs {
  u8 vdp_ctrl = 0;         // 0x11: bit 0 = display enable, bit 1 = Plane B enable
  u8 scroll_x = 0;         // 0x12: Plane A fine scroll X (pixels)
  u8 scroll_y = 0;         // 0x13: Plane A fine scroll Y (pixels)
  u8 plane_b_scroll_x = 0; // 0x14: Plane B fine scroll X (pixels) [Phase 10]
  u8 plane_b_scroll_y = 0; // 0x15: Plane B fine scroll Y (pixels) [Phase 10]
  u8 plane_a_base = 0;     // 0x16: Plane A tilemap base selector (VRAM page index, *1024)
  u8 plane_b_base = 0;     // 0x17: Plane B tilemap base selector (VRAM page index, *1024) [Phase 10]
  u8 pattern_base = 0;     // 0x18: Pattern base selector (shared by both planes, *1024)
};

// Phase 8: Palette debug tracking
struct PaletteDebugState {
  u8 pal_addr = 0;                  // Current PAL_ADDR value (0-255)
  u8 pal_index = 0;                 // Derived: pal_addr >> 1 (0-127)
  u8 pal_byte_sel = 0;              // Derived: pal_addr & 1 (0=low, 1=high)
  int last_write_frame = -1;        // Frame # of last palette write
  int last_write_scanline = -1;     // Scanline # of last palette write
  u8 last_write_entry = 0;          // Which entry (0-127) was last written
  u8 last_write_byte_sel = 0;       // Which byte (0=low, 1=high) was last written
  int last_commit_frame = -1;       // Frame # of last palette commit
  int last_commit_scanline = -1;    // Scanline # of last palette commit
};

struct DebugState {
  int last_scanline = -1;
  bool vblank_flag = false;
  u16 last_vblank_latch_scanline = 0xFFFF;
  u64 vblank_latch_count = 0;
  // Phase 7: Register visibility
  PpuRegs active_regs;
  PpuRegs pending_regs;
  // Phase 8: Palette debug state
  PaletteDebugState palette_debug;
};

class PPU {
 public:
  // Phase 7: VRAM size (48KB per spec)
  static constexpr size_t kVramSizeBytes = 49152;  // 48KB

  // Phase 8: Palette RAM capacity (128 entries, 9-bit RGB)
  static constexpr size_t kPaletteEntries = 128;
  static constexpr size_t kPaletteApertureBytes = 256;  // 128 entries * 2 bytes

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

  // Phase 8: Palette byte write (used by DMA engine for palette DMA)
  // Writes to staged palette; takes byte address (0-255)
  void PaletteWriteByte(u8 addr, u8 value);

  // Phase 8: Palette commit (called at scanline start)
  void PaletteCommitAtScanlineStart(int frame, int scanline);

  // Phase 8: Debug access to palette
  const std::array<u16, kPaletteEntries>& GetStagedPalette() const { return staged_pal_; }
  const std::array<u16, kPaletteEntries>& GetActivePalette() const { return active_pal_; }
  const std::array<u32, kPaletteEntries>& GetActiveRgb888() const { return active_rgb888_; }

  // Phase 8: Set current frame for debug tracking
  void SetCurrentFrame(u64 frame) { current_frame_ = frame; }

  DebugState GetDebugState() const;

 private:
  // Phase 7: Tile decode (packed 4bpp)
  // Returns palette index (0-15) for pixel (x, y) within tile
  u8 DecodeTilePixel(u16 tile_index, int x_in_tile, int y_in_tile) const;

  // Phase 7/10: Tilemap entry fetch (16-bit little-endian)
  // Returns tile index (bits 0-9); ignores flip/priority for Phase 7/10
  // tilemap_base: VRAM page index (*1024 to get address)
  u16 FetchTilemapEntry(int tile_x, int tile_y, u8 tilemap_base) const;

  // Phase 10: Render one scanline of a tile plane into palette index buffer
  // Parameterized for use with both Plane A and Plane B
  void RenderTilePlaneScanline(int scanline, u8 scroll_x, u8 scroll_y,
                               u8 tilemap_base, std::array<u8, 256>& out_line) const;

  // Phase 8: Map palette index to ARGB8888 color using active palette
  u32 PaletteToArgb(u8 palette_index) const;

  // Phase 8: Expand 9-bit packed RGB to 32-bit ARGB8888
  static u32 ExpandPaletteEntry(u16 packed);

  // Phase 7: Initialize test pattern in VRAM for startup display
  void InitTestPattern();

  // Phase 8: Initialize default palette entries
  void InitDefaultPalette();

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

  // Phase 8: Palette RAM (128 entries, 9-bit RGB packed as uint16_t)
  // Bit layout: bits 0-2 = R (0-7), bits 3-5 = G (0-7), bits 6-8 = B (0-7), bits 9-15 = 0
  std::array<u16, kPaletteEntries> staged_pal_{};   // Write target
  std::array<u16, kPaletteEntries> active_pal_{};   // Render target
  std::array<u32, kPaletteEntries> active_rgb888_{}; // Cached expanded colors

  // Phase 8: Palette I/O state
  u8 pal_addr_ = 0;  // 8-bit byte address into 256-byte palette aperture

  // Phase 8: Palette debug tracking
  int last_pal_write_frame_ = -1;
  int last_pal_write_scanline_ = -1;
  u8 last_pal_write_entry_ = 0;
  u8 last_pal_write_byte_sel_ = 0;
  int last_pal_commit_frame_ = -1;
  int last_pal_commit_scanline_ = -1;

  // Phase 8: Current frame number (for debug tracking)
  u64 current_frame_ = 0;

  // Phase 10: Scanline buffers for dual-plane compositing
  // Stores palette indices (0-127) before final palette lookup
  mutable std::array<u8, 256> line_plane_a_{};
  mutable std::array<u8, 256> line_plane_b_{};
};

}  // namespace sz::ppu

#endif
