#include "devices/ppu/PPU.h"

namespace sz::ppu {

void PPU::Reset() {
  last_scanline_ = -1;
  vblank_flag_ = false;
  last_vblank_latch_scanline_ = 0xFFFF;
  vblank_latch_count_ = 0;

  // Phase 6: Initialize VRAM (all zeros)
  vram_.assign(kVramSizeBytes, 0x00);

  // Phase 7: Initialize fixed palette (simple 16-color palette for testing)
  // Colors: black, dark blue, dark green, dark cyan, dark red, dark magenta,
  //         brown, light gray, dark gray, blue, green, cyan, red, magenta, yellow, white
  palette_ = {{
      0xFF000000u,  // 0: black
      0xFF0000AAu,  // 1: dark blue
      0xFF00AA00u,  // 2: dark green
      0xFF00AAAAu,  // 3: dark cyan
      0xFFAA0000u,  // 4: dark red
      0xFFAA00AAu,  // 5: dark magenta
      0xFFAA5500u,  // 6: brown
      0xFFAAAAAAu,  // 7: light gray
      0xFF555555u,  // 8: dark gray
      0xFF5555FFu,  // 9: blue
      0xFF55FF55u,  // 10: green
      0xFF55FFFFu,  // 11: cyan
      0xFFFF5555u,  // 12: red
      0xFFFF55FFu,  // 13: magenta
      0xFFFFFF55u,  // 14: yellow
      0xFFFFFFFFu,  // 15: white
  }};

  // Phase 7: Initialize test tiles and tilemap for startup display
  // This provides visible output without needing a ROM
  InitTestPattern();

  // Phase 7: Set up registers for display
  // Pattern base = page 0 (0x0000), Tilemap base = page 4 (0x1000)
  pending_regs_.pattern_base = 0;
  pending_regs_.plane_a_base = 4;
  pending_regs_.scroll_x = 0;
  pending_regs_.scroll_y = 0;
  pending_regs_.vdp_ctrl = 0x01;  // Display enabled
  active_regs_ = pending_regs_;
}

void PPU::InitTestPattern() {
  // Create 4 test tiles at VRAM 0x0000 (pattern base page 0)
  // Each tile is 8x8, 4bpp packed = 32 bytes
  // Layout: high nibble = left pixel, low nibble = right pixel

  size_t offset = 0;

  // Tile 0: Solid white (palette index 15)
  for (int i = 0; i < 32; ++i) {
    vram_[offset++] = 0xFF;  // Both pixels = 15
  }

  // Tile 1: Checkerboard (palette indices 0 and 15)
  for (int row = 0; row < 8; ++row) {
    u8 pattern = (row & 1) ? 0xF0 : 0x0F;
    for (int col = 0; col < 4; ++col) {
      vram_[offset++] = pattern;
    }
  }

  // Tile 2: Vertical stripes (palette indices 0 and 12=red)
  for (int i = 0; i < 32; ++i) {
    vram_[offset++] = 0x0C;  // Left=0, Right=12
  }

  // Tile 3: Horizontal stripes (palette indices 0 and 10=green)
  for (int row = 0; row < 8; ++row) {
    u8 pattern = (row & 1) ? 0xAA : 0x00;  // 0xAA = both pixels = 10
    for (int col = 0; col < 4; ++col) {
      vram_[offset++] = pattern;
    }
  }

  // Create tilemap at VRAM 0x1000 (tilemap base page 4)
  // 32x24 entries, 16-bit little-endian, repeating [0,1,2,3]
  size_t tilemap_base = 0x1000;
  int tile_index = 0;
  for (int entry = 0; entry < 32 * 24; ++entry) {
    size_t addr = tilemap_base + static_cast<size_t>(entry * 2);
    vram_[addr] = static_cast<u8>(tile_index);      // Low byte = tile index
    vram_[addr + 1] = 0x00;                          // High byte = 0
    tile_index = (tile_index + 1) & 0x03;            // Cycle 0,1,2,3
  }
}

u8 PPU::IoRead(u8 port) {
  switch (port) {
    case 0x10:  // VDP_STATUS (R)
      // Bit 0: VBlank flag (live)
      return vblank_flag_ ? 0x01 : 0x00;

    case 0x11:  // VDP_CTRL (R) - returns pending value
      return pending_regs_.vdp_ctrl;

    case 0x12:  // PLANE_A_SCROLL_X (R)
      return pending_regs_.scroll_x;

    case 0x13:  // PLANE_A_SCROLL_Y (R)
      return pending_regs_.scroll_y;

    case 0x16:  // PLANE_A_BASE (R)
      return pending_regs_.plane_a_base;

    case 0x18:  // PATTERN_BASE (R)
      return pending_regs_.pattern_base;

    default:
      return 0xFF;  // Unmapped ports return 0xFF
  }
}

void PPU::IoWrite(u8 port, u8 value) {
  // Phase 7 timing rule: all writes go to pending registers
  // They take effect at the next scanline boundary
  switch (port) {
    case 0x11:  // VDP_CTRL (W)
      pending_regs_.vdp_ctrl = value;
      break;

    case 0x12:  // PLANE_A_SCROLL_X (W)
      pending_regs_.scroll_x = value;
      break;

    case 0x13:  // PLANE_A_SCROLL_Y (W)
      pending_regs_.scroll_y = value;
      break;

    case 0x16:  // PLANE_A_BASE (W)
      pending_regs_.plane_a_base = value;
      break;

    case 0x18:  // PATTERN_BASE (W)
      pending_regs_.pattern_base = value;
      break;

    default:
      // Unmapped ports: writes ignored
      break;
  }
}

void PPU::BeginScanline(int scanline) {
  // Phase 7: Latch pending→active at start of every scanline
  active_regs_ = pending_regs_;

  // Phase 5: VBlank flag timing (exact per timing model)
  // vblank_flag = true at START of scanline 192
  // vblank_flag = false at START of scanline 0
  if (scanline == kVBlankStartScanline) {
    vblank_flag_ = true;
    last_vblank_latch_scanline_ = static_cast<u16>(scanline);
    vblank_latch_count_++;
  } else if (scanline == 0) {
    vblank_flag_ = false;
  }
}

void PPU::RenderScanline(int scanline, Framebuffer& fb) {
  last_scanline_ = scanline;

  // Only render visible scanlines (0-191)
  if (scanline < 0 || scanline >= kScreenHeight) {
    return;
  }

  // Check display enable (bit 0 of VDP_CTRL)
  const bool display_enable = (active_regs_.vdp_ctrl & 0x01) != 0;

  // Calculate framebuffer row offset
  const size_t row_offset = static_cast<size_t>(scanline * kScreenWidth);

  if (!display_enable) {
    // Display disabled: fill scanline with black (palette index 0)
    const u32 black = palette_[0];
    for (int x = 0; x < kScreenWidth; ++x) {
      fb.pixels[row_offset + static_cast<size_t>(x)] = black;
    }
    return;
  }

  // Phase 7: Render Plane A
  const int scroll_x = active_regs_.scroll_x;
  const int scroll_y = active_regs_.scroll_y;

  // Compute scrolled Y coordinate (wrap at 192 for Phase 7)
  const int global_y = (scanline + scroll_y) % kScreenHeight;
  const int tile_y = global_y / 8;
  const int y_in_tile = global_y % 8;

  for (int x = 0; x < kScreenWidth; ++x) {
    // Compute scrolled X coordinate (wrap at 256)
    const int global_x = (x + scroll_x) % kScreenWidth;
    const int tile_x = global_x / 8;
    const int x_in_tile = global_x % 8;

    // Fetch tilemap entry and get tile index
    const u16 tile_index = FetchTilemapEntry(tile_x, tile_y);

    // Decode tile pixel to get palette index (0-15)
    const u8 palette_index = DecodeTilePixel(tile_index, x_in_tile, y_in_tile);

    // Map palette index to ARGB8888 color
    const u32 color = PaletteToArgb(palette_index);

    // Store in framebuffer
    fb.pixels[row_offset + static_cast<size_t>(x)] = color;
  }
}

u8 PPU::DecodeTilePixel(u16 tile_index, int x_in_tile, int y_in_tile) const {
  // Phase 7: Packed 4bpp tile format
  // - Each tile is 8x8 pixels, 32 bytes total
  // - Each row is 4 bytes (2 pixels per byte)
  // - High nibble = left pixel (even X), low nibble = right pixel (odd X)
  //
  // Tile address = pattern_base * 1024 + tile_index * 32
  // Row offset = y_in_tile * 4
  // Byte offset within row = x_in_tile / 2
  // Nibble selection: high nibble if x_in_tile is even, low nibble if odd

  const u32 pattern_base_addr =
      static_cast<u32>(active_regs_.pattern_base) * 1024;
  const u32 tile_addr = pattern_base_addr + (static_cast<u32>(tile_index) * 32);
  const u32 row_offset = static_cast<u32>(y_in_tile) * 4;
  const u32 byte_offset = static_cast<u32>(x_in_tile) / 2;
  const u32 pixel_addr = tile_addr + row_offset + byte_offset;

  // Wrap VRAM address
  const u8 byte_value = vram_[pixel_addr % kVramSizeBytes];

  // Extract nibble: high nibble for even X, low nibble for odd X
  if ((x_in_tile & 1) == 0) {
    return (byte_value >> 4) & 0x0F;  // High nibble (left pixel)
  } else {
    return byte_value & 0x0F;  // Low nibble (right pixel)
  }
}

u16 PPU::FetchTilemapEntry(int tile_x, int tile_y) const {
  // Phase 7: 16-bit tilemap entries (little-endian)
  // - Tilemap is 32x24 tiles
  // - Entry address = tilemap_base * 1024 + (tile_y * 32 + tile_x) * 2
  // - Bits 0-9: tile index (0-1023)
  // - Bits 10-12: palette select (ignored in Phase 7)
  // - Bits 13-15: flip/priority (ignored in Phase 7)

  const u32 tilemap_base_addr =
      static_cast<u32>(active_regs_.plane_a_base) * 1024;
  const u32 entry_offset =
      static_cast<u32>((tile_y * 32 + tile_x) * 2);
  const u32 entry_addr = tilemap_base_addr + entry_offset;

  // Read 16-bit entry (little-endian)
  const u8 lo = vram_[entry_addr % kVramSizeBytes];
  const u8 hi = vram_[(entry_addr + 1) % kVramSizeBytes];
  const u16 entry = static_cast<u16>(lo) | (static_cast<u16>(hi) << 8);

  // Extract tile index (bits 0-9)
  return entry & 0x03FF;
}

u32 PPU::PaletteToArgb(u8 palette_index) const {
  // Phase 7: Direct lookup into fixed palette
  // Clamp index to palette size for safety
  if (palette_index >= kPaletteSize) {
    palette_index = 0;
  }
  return palette_[palette_index];
}

DebugState PPU::GetDebugState() const {
  DebugState state;
  state.last_scanline = last_scanline_;
  state.vblank_flag = vblank_flag_;
  state.last_vblank_latch_scanline = last_vblank_latch_scanline_;
  state.vblank_latch_count = vblank_latch_count_;
  state.active_regs = active_regs_;
  state.pending_regs = pending_regs_;
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
