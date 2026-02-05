# Super_Z80 Diagnostic Cartridge ROM

A diagnostic ROM for validating the Super_Z80 emulator implementation.
This ROM tests core emulator functionality: VBlank timing, IRQ handling,
DMA transfers, and basic video rendering.

## Building

### Requirements

- **sjasmplus** Z80 cross-assembler
  - Install on Arch Linux: `yay -S sjasmplus`
  - Or build from source: https://github.com/z00m128/sjasmplus

### Build Commands

```bash
cd build
make
```

Output: `build/out/diag_cart.bin`

### Verification

```bash
make info     # Show ROM size and verify limits
make disasm   # Generate disassembly listing
```

## ROM Specifications

| Property | Value |
|----------|-------|
| Entry Point | 0x0000 |
| ISR Vector | 0x0038 (IM 1) |
| Max Size | 32KB |
| Banking | None (bank 0 only) |

## Memory Map

### ROM (0x0000 - 0x7FFF)
- Reset vector and initialization code
- ISR handler at 0x0038
- Data tables (tiles, palette, tilemap)

### RAM Variables (0xC000 - 0xC3FF)
| Address | Name | Size | Description |
|---------|------|------|-------------|
| 0xC000 | FRAME_COUNTER | 16-bit | Incremented by main loop each VBlank |
| 0xC002 | VBLANK_COUNT | 16-bit | Incremented by ISR each VBlank |
| 0xC004 | ISR_ENTRY_COUNT | 16-bit | ISR entry counter (should equal VBLANK_COUNT) |
| 0xC006 | SCRATCH | 8-bit | ISR palette toggle state |
| 0xC100 | DMA_PAL_BUF | 32 bytes | Palette DMA source buffer |
| 0xC120 | DMA_TILE_BUF | 128 bytes | Tile pattern DMA source buffer |
| 0xC200 | TILEMAP_BUF | 1536 bytes | Tilemap DMA source buffer |

## Hardware Usage

### Video (Plane A only)
- 32x24 tilemap (256x192 pixels)
- 4 test tiles: solid, checkerboard, vertical stripes, horizontal stripes
- Pattern repeats [0 1 2 3] across each row

### Palette
- Entry 0: Black (background)
- Entry 1: Red/Blue (toggles each VBlank)
- Entry 2: Green
- Entry 3: Blue
- Entry 4: White
- Entries 5-15: Gradient/utility colors

### Interrupts
- VBlank IRQ enabled (bit 0)
- ISR at 0x0038:
  1. Increment counters
  2. Toggle palette entry 1 (Red ↔ Blue)
  3. Acknowledge IRQ
  4. RETI

## Expected Behavior

### Visual
- Stable 256x192 display with repeating tile pattern
- Tile 0 (solid) pulses between red and blue once per frame
- No flicker or tearing

### Timing
- ISR fires exactly once per VBlank
- FRAME_COUNTER and VBLANK_COUNT should track together
- VBlank starts at scanline 192

## Validation Criteria

| Check | Pass Condition |
|-------|----------------|
| Display | Correct 32x24 tilemap pattern visible |
| Palette pulse | Red ↔ Blue toggle exactly once per frame |
| ISR count | Increments exactly once per VBlank |
| No drift | Counters stay synchronized over minutes |
| No flicker | Stable image, no visual corruption |

## DMA Destination Convention

This ROM uses the following DMA destination convention:

- **DMA_CTRL bit 3 (DST_IS_PALETTE)**:
  - If set: DMA_DST is a byte offset into Palette RAM (0-255)
  - If clear: DMA_DST is a VRAM address

The emulator must honor this convention for the ROM to pass.

## Source Files

| File | Description |
|------|-------------|
| `src/diag_cart.asm` | Main ROM source |
| `src/hw.inc` | Hardware constants (ports, addresses) |
| `src/tiles.inc` | Tile pattern data (4 tiles, 4bpp) |
| `src/palette.inc` | Palette data (16 entries) |
| `src/tilemap.inc` | Tilemap data (32x24) |
