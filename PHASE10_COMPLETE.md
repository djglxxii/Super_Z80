# Phase 10 Complete: Plane B (Dual Background / Parallax)

## Overview

Phase 10 implements Plane B as a second tilemap layer with independent base address and scroll registers, enabling dual-plane parallax scrolling effects. Plane A serves as the foreground and Plane B as the background, with deterministic compositing rules.

## Deliverables

### 1. PPU Register Additions

New registers added to the Video Control block (0x10-0x1F):

| Port | Register | Description |
|------|----------|-------------|
| 0x14 | PLANE_B_SCROLL_X | Plane B horizontal scroll (0-255 pixels) |
| 0x15 | PLANE_B_SCROLL_Y | Plane B vertical scroll (0-255 pixels) |
| 0x17 | PLANE_B_BASE | Plane B tilemap VRAM base (page index, *1024) |

**VDP_CTRL (0x11) Bit Layout:**
| Bit | Name | Description |
|-----|------|-------------|
| 0 | Display Enable | Global display on/off |
| 1 | Plane B Enable | Plane B rendering on/off (Phase 10) |

All registers follow the pending→active latching semantics (changes take effect at start of next scanline).

### 2. Scanline Renderer Updates

**New Functions (`src/devices/ppu/PPU.cpp`):**

```cpp
// Reusable tile plane renderer
void RenderTilePlaneScanline(int scanline, u8 scroll_x, u8 scroll_y,
                             u8 tilemap_base, std::array<u8, 256>& out_line) const;

// Updated tilemap fetch with parameterized base
u16 FetchTilemapEntry(int tile_x, int tile_y, u8 tilemap_base) const;
```

**Scanline Buffers:**
- `line_plane_a_[256]` - Plane A palette indices
- `line_plane_b_[256]` - Plane B palette indices

**Rendering Pipeline (per visible scanline):**
1. At scanline start: latch pending→active registers
2. If display disabled: output black
3. Render Plane B into `line_plane_b_` (if enabled, else fill with 0)
4. Render Plane A into `line_plane_a_`
5. Composite: Plane A wins if non-transparent (index != 0), else Plane B
6. Palette lookup to final ARGB framebuffer

### 3. Compositing Rules

**Priority Order:** Plane A (front) > Plane B (back)

**Transparency:** Palette index 0 is treated as transparent during compositing.

**Algorithm:**
```cpp
for (int x = 0; x < 256; ++x) {
    u8 final_index = (pixel_a != 0) ? pixel_a : pixel_b;
    framebuffer[x] = PaletteToArgb(final_index);
}
```

### 4. Test ROM

Created `/roms/phase10/test_plane_b.asm`:

**Features:**
- 6 distinct tile patterns (transparent, solid, border, checkerboard, diagonal, crosshatch)
- Plane A: Sparse foreground elements with transparent gaps
- Plane B: Continuous background pattern (tiles 3-5 cycling)
- Two separate tilemaps at different VRAM pages (A at 0x1000, B at 0x2000)
- Parallax animation: Plane B scrolls slowly, Plane A remains static
- Custom palette with distinct foreground/background colors

**VRAM Layout:**
| Address | Content |
|---------|---------|
| 0x0000 | Pattern data (6 tiles, 192 bytes) |
| 0x1000 | Plane A tilemap (32x24, 1536 bytes) |
| 0x2000 | Plane B tilemap (32x24, 1536 bytes) |

**Expected Behavior:**
- Two distinct layers visible simultaneously
- Plane B scrolls diagonally behind Plane A
- Where Plane A has non-transparent pixels, Plane B is hidden
- No tearing or corruption

### 5. Debug UI Enhancements

**PPU Panel Updates (`src/debugui/panels/PanelPPU.cpp`):**

- Updated header: "Phase 10: PPU Dual Plane (A + B) + Palette RAM"
- VDP_CTRL section now shows "Plane B En: ON/OFF"
- New register rows:
  - B Scroll X (0x14) - pending/active values
  - B Scroll Y (0x15) - pending/active values
  - PlaneB Base (0x17) - page index and VRAM address
- VRAM Viewer: Added "Plane B Map" quick-jump button

## Files Modified

### PPU Core
| File | Changes |
|------|---------|
| `src/devices/ppu/PPU.h` | Added Plane B fields to `PpuRegs`, line buffers, function declarations |
| `src/devices/ppu/PPU.cpp` | I/O handlers for 0x14/0x15/0x17, `RenderTilePlaneScanline()`, compositing logic |

### Debug UI
| File | Changes |
|------|---------|
| `src/debugui/panels/PanelPPU.h` | Added `tilemap_plane_select_` state variable |
| `src/debugui/panels/PanelPPU.cpp` | Plane B register display, quick-jump button |

## Files Created

| File | Description |
|------|-------------|
| `roms/phase10/test_plane_b.asm` | Phase 10 test ROM assembly source |
| `roms/phase10/Makefile` | Build file (requires z80asm) |

## Backward Compatibility

- Existing ROMs continue to work unchanged
- Plane B is disabled by default (VDP_CTRL bit 1 = 0)
- When VDP_CTRL = 0x01 (display enable only), behavior matches Phase 9
- Diagnostic ROM from Phase 9 remains fully functional

## Acceptance Criteria

### Register Functionality
- [x] PLANE_B_BASE (0x17) selects different tilemap region without affecting Plane A
- [x] PLANE_B_SCROLL_X/Y (0x14/0x15) move only Plane B
- [x] Boundary-applied writes: changes take effect at next scanline start

### Compositing
- [x] Palette index 0 treated as transparent
- [x] Plane A pixels (non-zero) always visible over Plane B
- [x] Plane B shows through transparent areas of Plane A

### No Regressions
- [x] Plane A rendering unchanged when Plane B disabled
- [x] Phase 9 diagnostic ROM continues to pass
- [x] Project compiles without errors

## Usage

### Building the Test ROM
```bash
cd roms/phase10
make  # Requires z80asm
```

### Running with Test ROM
```bash
./build/superz80_app --rom roms/phase10/test_plane_b.bin
```

### Enabling Plane B in Code
```z80
; Enable display + Plane B
ld a, 0x03      ; bit 0 = display, bit 1 = Plane B
out (0x11), a   ; VDP_CTRL

; Set Plane B tilemap base (page 8 = VRAM 0x2000)
ld a, 8
out (0x17), a   ; PLANE_B_BASE

; Set Plane B scroll
ld a, 10
out (0x14), a   ; PLANE_B_SCROLL_X
ld a, 5
out (0x15), a   ; PLANE_B_SCROLL_Y
```

## Notes

- Both planes share the same pattern base (0x18) - separate pattern bases deferred to future phase
- Per-tile priority bits (from tilemap entry high byte) not yet implemented - global plane ordering only
- Tile format remains 8x8, 4bpp (16 colors per tile)
- Tilemap format remains 32x24 tiles, 16-bit entries
