#include "devices/ppu/PPU.h"

namespace sz::ppu {

void PPU::Reset() {
  last_scanline_ = -1;
  vblank_flag_ = false;
  last_vblank_latch_scanline_ = 0xFFFF;
  vblank_latch_count_ = 0;

  // Phase 6: Initialize VRAM (all zeros)
  vram_.assign(kVramSizeBytes, 0x00);

  // Phase 8: Initialize palette RAM with default colors
  InitDefaultPalette();

  // Phase 8: Reset palette I/O state
  pal_addr_ = 0;
  last_pal_write_frame_ = -1;
  last_pal_write_scanline_ = -1;
  last_pal_write_entry_ = 0;
  last_pal_write_byte_sel_ = 0;
  last_pal_commit_frame_ = -1;
  last_pal_commit_scanline_ = -1;
  current_frame_ = 0;

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

void PPU::InitDefaultPalette() {
  // Phase 8: Initialize palette with default 16-color palette (CGA-style)
  // Pack 9-bit RGB as: bits 0-2 = R, bits 3-5 = G, bits 6-8 = B
  // Each color component is 3-bit (0-7)

  // Helper lambda to pack RGB
  auto pack_rgb = [](int r, int g, int b) -> u16 {
    return static_cast<u16>((r & 0x7) | ((g & 0x7) << 3) | ((b & 0x7) << 6));
  };

  // Default colors (3-bit per channel, 0-7 scale):
  staged_pal_[0] = pack_rgb(0, 0, 0);   // 0: black
  staged_pal_[1] = pack_rgb(0, 0, 5);   // 1: dark blue
  staged_pal_[2] = pack_rgb(0, 5, 0);   // 2: dark green
  staged_pal_[3] = pack_rgb(0, 5, 5);   // 3: dark cyan
  staged_pal_[4] = pack_rgb(5, 0, 0);   // 4: dark red
  staged_pal_[5] = pack_rgb(5, 0, 5);   // 5: dark magenta
  staged_pal_[6] = pack_rgb(5, 3, 0);   // 6: brown
  staged_pal_[7] = pack_rgb(5, 5, 5);   // 7: light gray
  staged_pal_[8] = pack_rgb(3, 3, 3);   // 8: dark gray
  staged_pal_[9] = pack_rgb(3, 3, 7);   // 9: blue
  staged_pal_[10] = pack_rgb(3, 7, 3);  // 10: green
  staged_pal_[11] = pack_rgb(3, 7, 7);  // 11: cyan
  staged_pal_[12] = pack_rgb(7, 3, 3);  // 12: red
  staged_pal_[13] = pack_rgb(7, 3, 7);  // 13: magenta
  staged_pal_[14] = pack_rgb(7, 7, 3);  // 14: yellow
  staged_pal_[15] = pack_rgb(7, 7, 7);  // 15: white

  // Fill remaining entries with black
  for (size_t i = 16; i < kPaletteEntries; ++i) {
    staged_pal_[i] = 0;
  }

  // Copy to active and expand to RGB888
  active_pal_ = staged_pal_;
  for (size_t i = 0; i < kPaletteEntries; ++i) {
    active_rgb888_[i] = ExpandPaletteEntry(active_pal_[i]);
  }
}

u32 PPU::ExpandPaletteEntry(u16 packed) {
  // Extract 3-bit components from packed 9-bit RGB
  // Bit layout: bits 0-2 = R, bits 3-5 = G, bits 6-8 = B
  int r3 = packed & 0x7;
  int g3 = (packed >> 3) & 0x7;
  int b3 = (packed >> 6) & 0x7;

  // Expand 3-bit to 8-bit: r8 = (r3 * 255) / 7
  int r8 = (r3 * 255) / 7;
  int g8 = (g3 * 255) / 7;
  int b8 = (b3 * 255) / 7;

  // Pack as ARGB8888 (A=255)
  return 0xFF000000u |
         (static_cast<u32>(r8) << 16) |
         (static_cast<u32>(g8) << 8) |
         static_cast<u32>(b8);
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

    case 0x1E: {  // PAL_ADDR (R) - returns current palette byte address
      return pal_addr_;
    }

    case 0x1F: {  // PAL_DATA (R) - read byte at current pal_addr, auto-increment
      u8 pal_index = pal_addr_ >> 1;     // Entry index (0-127)
      u8 byte_sel = pal_addr_ & 1;       // 0=low byte, 1=high byte

      u16 packed = staged_pal_[pal_index];
      u8 result;
      if (byte_sel == 0) {
        result = static_cast<u8>(packed & 0xFF);       // Low byte (bits 0-7)
      } else {
        result = static_cast<u8>((packed >> 8) & 0xFF); // High byte (bits 8-15)
      }

      // Auto-increment by +1 byte after read
      pal_addr_++;  // Wraps 0xFF -> 0x00 naturally

      return result;
    }

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

    case 0x1E:  // PAL_ADDR (W) - set palette byte address
      pal_addr_ = value;
      break;

    case 0x1F: {  // PAL_DATA (W) - write byte at current pal_addr, auto-increment
      PaletteWriteByte(pal_addr_, value);

      // Auto-increment by +1 byte after write
      pal_addr_++;  // Wraps 0xFF -> 0x00 naturally
      break;
    }

    default:
      // Unmapped ports: writes ignored
      break;
  }
}

void PPU::BeginScanline(int scanline) {
  // Phase 7: Latch pending→active at start of every scanline
  active_regs_ = pending_regs_;

  // Phase 8: Commit palette at scanline start (before rendering)
  PaletteCommitAtScanlineStart(static_cast<int>(current_frame_), scanline);

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

void PPU::PaletteWriteByte(u8 addr, u8 value) {
  // Phase 8: Write one byte to staged palette
  // addr is 0-255 byte address into palette aperture (128 entries * 2 bytes)
  u8 pal_index = addr >> 1;      // Entry index (0-127)
  u8 byte_sel = addr & 1;        // 0=low byte, 1=high byte

  u16 current = staged_pal_[pal_index];

  if (byte_sel == 0) {
    // Low byte: bits 0-7
    current = (current & 0xFF00) | value;
  } else {
    // High byte: bits 8-15 (only bit 0 is meaningful for B2)
    current = (current & 0x00FF) | (static_cast<u16>(value) << 8);
  }

  // Mask to 9 bits (bits 9-15 are always 0)
  staged_pal_[pal_index] = current & 0x01FF;

  // Debug tracking
  last_pal_write_frame_ = static_cast<int>(current_frame_);
  last_pal_write_scanline_ = last_scanline_;
  last_pal_write_entry_ = pal_index;
  last_pal_write_byte_sel_ = byte_sel;
}

void PPU::PaletteCommitAtScanlineStart(int frame, int scanline) {
  // Phase 8: Copy staged palette to active palette at scanline start
  // This ensures no mid-scanline tearing
  active_pal_ = staged_pal_;

  // Expand to RGB888 for rendering
  for (size_t i = 0; i < kPaletteEntries; ++i) {
    active_rgb888_[i] = ExpandPaletteEntry(active_pal_[i]);
  }

  // Debug tracking
  last_pal_commit_frame_ = frame;
  last_pal_commit_scanline_ = scanline;
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
    const u32 black = active_rgb888_[0];  // Phase 8: Use active palette
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
  // Phase 8: Lookup in active palette (cached RGB888)
  // Clamp index to palette entries for safety
  if (palette_index >= kPaletteEntries) {
    palette_index = 0;
  }
  return active_rgb888_[palette_index];
}

DebugState PPU::GetDebugState() const {
  DebugState state;
  state.last_scanline = last_scanline_;
  state.vblank_flag = vblank_flag_;
  state.last_vblank_latch_scanline = last_vblank_latch_scanline_;
  state.vblank_latch_count = vblank_latch_count_;
  state.active_regs = active_regs_;
  state.pending_regs = pending_regs_;

  // Phase 8: Palette debug state
  state.palette_debug.pal_addr = pal_addr_;
  state.palette_debug.pal_index = pal_addr_ >> 1;
  state.palette_debug.pal_byte_sel = pal_addr_ & 1;
  state.palette_debug.last_write_frame = last_pal_write_frame_;
  state.palette_debug.last_write_scanline = last_pal_write_scanline_;
  state.palette_debug.last_write_entry = last_pal_write_entry_;
  state.palette_debug.last_write_byte_sel = last_pal_write_byte_sel_;
  state.palette_debug.last_commit_frame = last_pal_commit_frame_;
  state.palette_debug.last_commit_scanline = last_pal_commit_scanline_;

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
