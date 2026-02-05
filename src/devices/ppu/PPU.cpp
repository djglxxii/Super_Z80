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

  // Phase 7/10: Set up registers for display
  // Pattern base = page 0 (0x0000), Plane A tilemap base = page 4 (0x1000)
  pending_regs_.pattern_base = 0;
  pending_regs_.plane_a_base = 4;
  pending_regs_.scroll_x = 0;
  pending_regs_.scroll_y = 0;
  // Phase 10: Plane B defaults (disabled, separate tilemap base)
  pending_regs_.plane_b_base = 8;        // Page 8 (0x2000) - distinct from Plane A
  pending_regs_.plane_b_scroll_x = 0;
  pending_regs_.plane_b_scroll_y = 0;
  pending_regs_.vdp_ctrl = 0x01;         // Display enabled, Plane B disabled (bit 1 = 0)
  active_regs_ = pending_regs_;

  // Phase 11: Reset sprite state
  pending_sprite_regs_.spr_ctrl = 0;     // Sprites disabled
  pending_sprite_regs_.sat_base = 0;     // SAT at VRAM 0x0000
  active_sprite_regs_ = pending_sprite_regs_;
  sprite_overflow_latch_ = false;
  last_sprite_selection_ = {};
  line_sprites_.fill({});
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

    case 0x14:  // PLANE_B_SCROLL_X (R) [Phase 10]
      return pending_regs_.plane_b_scroll_x;

    case 0x15:  // PLANE_B_SCROLL_Y (R) [Phase 10]
      return pending_regs_.plane_b_scroll_y;

    case 0x16:  // PLANE_A_BASE (R)
      return pending_regs_.plane_a_base;

    case 0x17:  // PLANE_B_BASE (R) [Phase 10]
      return pending_regs_.plane_b_base;

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

    // Phase 11: Sprite I/O ports (0x20-0x2F)
    case 0x20:  // SPR_CTRL (R)
      return pending_sprite_regs_.spr_ctrl;

    case 0x21:  // SAT_BASE (R)
      return pending_sprite_regs_.sat_base;

    case 0x22:  // SPR_STATUS (R)
      // Bit 0: Overflow latch (persists until VBlank start)
      return sprite_overflow_latch_ ? 0x01 : 0x00;

    case 0x23:  // Reserved
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F:
      return 0xFF;  // Reserved ports return 0xFF

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

    case 0x14:  // PLANE_B_SCROLL_X (W) [Phase 10]
      pending_regs_.plane_b_scroll_x = value;
      break;

    case 0x15:  // PLANE_B_SCROLL_Y (W) [Phase 10]
      pending_regs_.plane_b_scroll_y = value;
      break;

    case 0x16:  // PLANE_A_BASE (W)
      pending_regs_.plane_a_base = value;
      break;

    case 0x17:  // PLANE_B_BASE (W) [Phase 10]
      pending_regs_.plane_b_base = value;
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

    // Phase 11: Sprite I/O ports (0x20-0x2F)
    case 0x20:  // SPR_CTRL (W)
      pending_sprite_regs_.spr_ctrl = value;
      break;

    case 0x21:  // SAT_BASE (W)
      pending_sprite_regs_.sat_base = value;
      break;

    case 0x22:  // SPR_STATUS (R) - writes ignored (read-only status register)
    case 0x23:  // Reserved ports - writes ignored
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F:
      break;

    default:
      // Unmapped ports: writes ignored
      break;
  }
}

void PPU::BeginScanline(int scanline) {
  // Phase 7: Latch pending→active at start of every scanline
  active_regs_ = pending_regs_;

  // Phase 11: Latch sprite registers at scanline start
  active_sprite_regs_ = pending_sprite_regs_;

  // Phase 8: Commit palette at scanline start (before rendering)
  PaletteCommitAtScanlineStart(static_cast<int>(current_frame_), scanline);

  // Phase 5: VBlank flag timing (exact per timing model)
  // vblank_flag = true at START of scanline 192
  // vblank_flag = false at START of scanline 0
  if (scanline == kVBlankStartScanline) {
    vblank_flag_ = true;
    last_vblank_latch_scanline_ = static_cast<u16>(scanline);
    vblank_latch_count_++;
    // Phase 11: Clear sprite overflow latch at VBlank start
    sprite_overflow_latch_ = false;
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
  // Phase 10: Check Plane B enable (bit 1 of VDP_CTRL)
  const bool plane_b_enable = (active_regs_.vdp_ctrl & 0x02) != 0;

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

  // Phase 10: Dual-plane rendering with compositing
  // Step 1: Render Plane B (background) into line buffer if enabled
  if (plane_b_enable) {
    RenderTilePlaneScanline(scanline,
                            active_regs_.plane_b_scroll_x,
                            active_regs_.plane_b_scroll_y,
                            active_regs_.plane_b_base,
                            line_plane_b_);
  } else {
    // Plane B disabled: fill with transparent (palette index 0)
    line_plane_b_.fill(0);
  }

  // Step 2: Render Plane A (foreground) into line buffer
  // Plane A is always enabled when display is enabled (backward compatibility)
  RenderTilePlaneScanline(scanline,
                          active_regs_.scroll_x,
                          active_regs_.scroll_y,
                          active_regs_.plane_a_base,
                          line_plane_a_);

  // Phase 11: Evaluate and render sprites for this scanline
  SpriteScanlineSelection sprite_selection = EvaluateSpritesForScanline(scanline);

  // Set overflow latch if this scanline had more than 16 sprites
  if (sprite_selection.overflowThisLine) {
    sprite_overflow_latch_ = true;
  }

  // Store selection for debug purposes
  last_sprite_selection_ = sprite_selection;

  // Render sprites into line buffer
  RenderSpriteLine(scanline, sprite_selection, line_sprites_);

  // Step 3: Composite all layers (Plane B, Plane A, Sprites)
  // Phase 11 compositing rules:
  //   - Normal sprites (behindPlaneA=0): appear above both planes
  //   - Behind sprites (behindPlaneA=1): appear between B and A (occluded by A)
  for (int x = 0; x < kScreenWidth; ++x) {
    const u8 pixel_a = line_plane_a_[static_cast<size_t>(x)];
    const u8 pixel_b = line_plane_b_[static_cast<size_t>(x)];
    const SpritePixel& pixel_s = line_sprites_[static_cast<size_t>(x)];

    u8 final_index;

    if (!pixel_s.opaque) {
      // No sprite pixel: use background composite (A over B)
      final_index = (pixel_a != 0) ? pixel_a : pixel_b;
    } else if (!pixel_s.behindPlaneA) {
      // Normal sprite priority: sprite above both planes
      // Use sprite palette: palette_base + color_index
      // Each palette has 16 entries, palette_sel selects which 16-entry bank
      final_index = static_cast<u8>((pixel_s.paletteSel << 4) | pixel_s.colorIndex);
    } else {
      // Behind sprite: sprite between B and A
      // If Plane A is opaque, A wins. Otherwise sprite wins over B.
      if (pixel_a != 0) {
        final_index = pixel_a;
      } else {
        final_index = static_cast<u8>((pixel_s.paletteSel << 4) | pixel_s.colorIndex);
      }
    }

    // Map palette index to ARGB8888 color and store in framebuffer
    fb.pixels[row_offset + static_cast<size_t>(x)] = PaletteToArgb(final_index);
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

u16 PPU::FetchTilemapEntry(int tile_x, int tile_y, u8 tilemap_base) const {
  // Phase 7/10: 16-bit tilemap entries (little-endian)
  // - Tilemap is 32x24 tiles
  // - Entry address = tilemap_base * 1024 + (tile_y * 32 + tile_x) * 2
  // - Bits 0-9: tile index (0-1023)
  // - Bits 10-12: palette select (ignored in Phase 7/10)
  // - Bits 13-15: flip/priority (ignored in Phase 7/10)

  const u32 tilemap_base_addr = static_cast<u32>(tilemap_base) * 1024;
  const u32 entry_offset = static_cast<u32>((tile_y * 32 + tile_x) * 2);
  const u32 entry_addr = tilemap_base_addr + entry_offset;

  // Read 16-bit entry (little-endian)
  const u8 lo = vram_[entry_addr % kVramSizeBytes];
  const u8 hi = vram_[(entry_addr + 1) % kVramSizeBytes];
  const u16 entry = static_cast<u16>(lo) | (static_cast<u16>(hi) << 8);

  // Extract tile index (bits 0-9)
  return entry & 0x03FF;
}

void PPU::RenderTilePlaneScanline(int scanline, u8 scroll_x, u8 scroll_y,
                                  u8 tilemap_base,
                                  std::array<u8, 256>& out_line) const {
  // Phase 10: Render one scanline of a tile plane into palette index buffer
  // This function is shared by both Plane A and Plane B rendering

  // Compute scrolled Y coordinate (wrap at screen height)
  const int global_y = (scanline + scroll_y) % kScreenHeight;
  const int tile_y = global_y / 8;
  const int y_in_tile = global_y % 8;

  for (int x = 0; x < kScreenWidth; ++x) {
    // Compute scrolled X coordinate (wrap at screen width)
    const int global_x = (x + scroll_x) % kScreenWidth;
    const int tile_x = global_x / 8;
    const int x_in_tile = global_x % 8;

    // Fetch tilemap entry and get tile index
    const u16 tile_index = FetchTilemapEntry(tile_x, tile_y, tilemap_base);

    // Decode tile pixel to get palette index (0-15)
    const u8 palette_index = DecodeTilePixel(tile_index, x_in_tile, y_in_tile);

    // Store palette index in output buffer
    out_line[static_cast<size_t>(x)] = palette_index;
  }
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

  // Phase 11: Sprite debug state
  state.sprite_debug = GetSpriteDebugState();

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

// Phase 11: Decode a single SAT entry from VRAM
SpriteEntry PPU::DecodeSATEntry(int index) const {
  SpriteEntry entry{};
  if (index < 0 || index >= kSatEntries) {
    return entry;  // Return zeroed entry for invalid index
  }

  // Calculate SAT address: sat_base * 256 + index * 8
  const u32 sat_base_addr = static_cast<u32>(active_sprite_regs_.sat_base) * 256;
  const u32 entry_addr = (sat_base_addr + static_cast<u32>(index * kSatEntrySize)) % kVramSizeBytes;

  // Read 8 bytes from VRAM
  entry.y = vram_[entry_addr];
  entry.x = vram_[(entry_addr + 1) % kVramSizeBytes];

  u8 tile_lo = vram_[(entry_addr + 2) % kVramSizeBytes];
  u8 tile_hi = vram_[(entry_addr + 3) % kVramSizeBytes];
  entry.tile = static_cast<u16>(tile_lo) | (static_cast<u16>(tile_hi & 0x0F) << 8);

  u8 attr = vram_[(entry_addr + 4) % kVramSizeBytes];
  entry.palette = attr & 0x0F;
  entry.behindPlaneA = (attr & 0x10) != 0;
  entry.flipX = (attr & 0x20) != 0;
  entry.flipY = (attr & 0x40) != 0;

  return entry;
}

// Phase 11: Evaluate which sprites intersect a given scanline
SpriteScanlineSelection PPU::EvaluateSpritesForScanline(int scanline) const {
  SpriteScanlineSelection selection{};
  selection.scanline = static_cast<u16>(scanline);

  // Check if sprites are enabled
  if ((active_sprite_regs_.spr_ctrl & 0x01) == 0) {
    return selection;  // Sprites disabled
  }

  // Iterate through all 48 SAT entries in order
  for (int i = 0; i < kSatEntries; ++i) {
    SpriteEntry sprite = DecodeSATEntry(i);

    // Check if sprite intersects this scanline
    // Using wrap-around semantics: dy = (uint8_t)(scanline - spriteY)
    // Intersects if dy < height
    u8 dy = static_cast<u8>(static_cast<u8>(scanline) - sprite.y);
    if (dy < kSpriteHeight) {
      // Sprite intersects this scanline
      if (selection.count < kMaxSpritesPerScanline) {
        selection.indices[selection.count] = static_cast<u8>(i);
        selection.count++;
      } else {
        // Found the 17th sprite - set overflow flag
        selection.overflowThisLine = true;
        break;  // Don't add further sprites
      }
    }
  }

  return selection;
}

// Phase 11: Decode a sprite tile pixel (reuses tile format from background)
u8 PPU::DecodeSpritePixel(u16 tile_index, int x_in_tile, int y_in_tile) const {
  // Same format as background tiles: 4bpp packed
  // Each tile is 8x8 pixels, 32 bytes total
  // High nibble = left pixel (even X), low nibble = right pixel (odd X)

  const u32 pattern_base_addr = static_cast<u32>(active_regs_.pattern_base) * 1024;
  const u32 tile_addr = pattern_base_addr + (static_cast<u32>(tile_index) * 32);
  const u32 row_offset = static_cast<u32>(y_in_tile) * 4;
  const u32 byte_offset = static_cast<u32>(x_in_tile) / 2;
  const u32 pixel_addr = tile_addr + row_offset + byte_offset;

  const u8 byte_value = vram_[pixel_addr % kVramSizeBytes];

  if ((x_in_tile & 1) == 0) {
    return (byte_value >> 4) & 0x0F;  // High nibble
  } else {
    return byte_value & 0x0F;  // Low nibble
  }
}

// Phase 11: Render selected sprites into a line buffer
void PPU::RenderSpriteLine(int scanline, const SpriteScanlineSelection& selection,
                           std::array<SpritePixel, 256>& out_line) const {
  // Clear the line buffer
  out_line.fill({});

  if (selection.count == 0) {
    return;
  }

  // Render sprites in REVERSE order (last selected → first selected)
  // This ensures that earlier SAT indices (higher priority) are drawn last
  // and thus appear on top
  for (int i = selection.count - 1; i >= 0; --i) {
    int sat_index = selection.indices[i];
    SpriteEntry sprite = DecodeSATEntry(sat_index);

    // Calculate which row of the sprite we're rendering
    u8 dy = static_cast<u8>(static_cast<u8>(scanline) - sprite.y);

    // Apply vertical flip if needed
    int src_y = sprite.flipY ? (kSpriteHeight - 1 - dy) : dy;

    // Render each pixel of the sprite
    for (int sx = 0; sx < kSpriteWidth; ++sx) {
      // Screen X position (wraps naturally with uint8)
      u8 screen_x = static_cast<u8>(sprite.x + sx);

      // Apply horizontal flip if needed
      int src_x = sprite.flipX ? (kSpriteWidth - 1 - sx) : sx;

      // Decode the tile pixel
      u8 color_index = DecodeSpritePixel(sprite.tile, src_x, src_y);

      // Color index 0 is transparent
      if (color_index == 0) {
        continue;
      }

      // Write to the line buffer (unconditionally overwrite since we're
      // rendering in reverse priority order - higher priority sprites
      // drawn last will naturally overwrite lower priority)
      SpritePixel& pixel = out_line[screen_x];
      pixel.opaque = true;
      pixel.colorIndex = color_index;
      pixel.paletteSel = sprite.palette;
      pixel.behindPlaneA = sprite.behindPlaneA;
    }
  }
}

// Phase 11: Get sprite debug state
const SpriteDebugState& PPU::GetSpriteDebugState() const {
  static SpriteDebugState state;

  state.enabled = (active_sprite_regs_.spr_ctrl & 0x01) != 0;
  state.spr_ctrl = active_sprite_regs_.spr_ctrl;
  state.sat_base = active_sprite_regs_.sat_base;
  state.overflowLatched = sprite_overflow_latch_;
  state.lastSelection = last_sprite_selection_;

  // Decode all 48 sprite entries for debug view
  for (int i = 0; i < kSatEntries; ++i) {
    state.sprites[i] = DecodeSATEntry(i);
  }

  return state;
}

}  // namespace sz::ppu
