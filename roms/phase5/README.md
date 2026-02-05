# Phase 5 VBlank Timing Test ROM

## Overview
This directory contains the test ROM for Phase 5: VBlank Timing validation.

## Files
- `test_vblank.asm` - Z80 assembly source for VBlank IRQ test ROM
- `test_vblank_masked.asm` - Z80 assembly source for masked VBlank test (negative test)

## ROM Requirements (Per Phase 5 Specification)

### Primary Test ROM: `test_vblank.asm`
The test ROM must:

1. **Initial Setup**:
   - `DI` (disable interrupts during setup)
   - Initialize SP near top of RAM (e.g., 0xFFFE)
   - Clear RAM counters:
     - `ISR_VBLANK_COUNT` at 0xC000 (incremented in ISR)
     - `FRAME_COUNT_MAIN` at 0xC002 (incremented in main loop)
   - Set interrupt mode: `IM 1` (vectors to 0x0038)

2. **Enable VBlank IRQ**:
   - Write to `IRQ_ENABLE` port (0x81) with bit 0 = 1 (enable VBlank only)
   - `EI` (enable interrupts, honor EI delay)

3. **ISR at 0x0038**:
   On entry:
   - Increment `ISR_VBLANK_COUNT` in RAM (0xC000)
   - Acknowledge VBlank pending: write `0x01` to `IRQ_ACK` port (0x82)
   - `RETI`

4. **Main Loop**:
   - Busy loop forever (DO NOT use `HALT` to avoid masking timing bugs)
   - Optionally: poll `VDP_STATUS` (port 0x10) to detect VBlank transitions
   - Optionally: copy `ISR_VBLANK_COUNT` to `FRAME_COUNT_MAIN` periodically

### Negative Test ROM: `test_vblank_masked.asm`
The negative test ROM must:

1. Leave VBlank IRQ masked (`IRQ_ENABLE` bit 0 = 0)
2. Poll `VDP_STATUS` (port 0x10) to confirm VBlank flag toggles correctly
3. Verify `ISR_VBLANK_COUNT` stays at 0 indefinitely (no ISR execution)

## Assembly and Loading
To assemble these ROMs (once z80asm or similar is available):
```bash
z80asm -o test_vblank.bin test_vblank.asm
z80asm -o test_vblank_masked.bin test_vblank_masked.asm
```

## Current Status (Phase 5)

### ✅ IMPLEMENTED:
- **PPU VBlank State**:
  - `vblank_flag` toggles at scanlines 0 and 192
  - `OnScanlineStart()` boundary hook implemented
  - Debug counters: `last_vblank_latch_scanline`, `vblank_latch_count`

- **VBlank IRQ Source**:
  - VBlank pending (IRQ_STATUS bit 0) latched at scanline 192 start
  - IRQController wired to receive VBlank IRQ from Console
  - Synthetic IRQ trigger from Phase 4 removed

- **VDP_STATUS Port (0x10)**:
  - Returns live view of `vblank_flag` in bit 0
  - Reading does NOT clear VBlank or IRQ bits
  - Clearing is via `IRQ_ACK` (port 0x82) only

- **Scheduler Integration**:
  - VBlank moved from Scheduler to PPU (proper ownership)
  - Scheduler calls Console `OnScanlineStart()` before CPU execution
  - Console calls PPU `OnScanlineStart()` for flag transitions
  - Console raises VBlank IRQ at scanline 192

- **Debug Visibility**:
  - PanelPPU shows: `vblank_flag`, `last_vblank_latch_scanline`, `vblank_latch_count`
  - PanelIRQ shows: VBlank pending bit, enable mask, /INT state
  - All state changes observable in real-time

### ⚠️ PENDING:
- **CPU Core**: Z80CpuStub does not execute instructions
  - Full validation requires real Z80 CPU core integration
  - ISR entry detection needs PC tracking

- **ROM Loading**: Infrastructure not yet in place
  - Need cartridge ROM loading
  - Need memory initialization from ROM file

## Validation Requirements (Deterministic Long-Run Tests)

When real Z80 CPU is integrated, validate over tens of thousands of frames:

### Assertions (Must Hold):
1. **VBlank Flag Timing**:
   - `vblank_flag` becomes true exactly at scanline 192 start
   - `vblank_flag` becomes false exactly at scanline 0 start

2. **With VBlank Enabled** (`IRQ_ENABLE` bit 0 = 1):
   - ISR entry count increments **exactly once per frame**
   - No double IRQs, no missed frames
   - `ISR_VBLANK_COUNT` matches frame count exactly

3. **With VBlank Masked** (`IRQ_ENABLE` bit 0 = 0):
   - `vblank_flag` still toggles correctly at scanlines 0/192
   - ISR never runs (`ISR_VBLANK_COUNT == 0`)
   - `/INT` remains deasserted

4. **IRQ Latching Semantics**:
   - VBlank pending (IRQ_STATUS bit 0) set at scanline 192 start
   - `/INT` asserts only when pending & enabled
   - `/INT` remains asserted until `IRQ_ACK` clears pending bit
   - `/INT` drops immediately after `IRQ_ACK` write

5. **No Unrelated Subsystem Changes**:
   - Rendering path remains Phase 0 test pattern only
   - No DMA/APU introduced
   - Scheduler scanline timing unchanged from Phase 3

### Pass/Fail Mapping to Phase 5 Checklist

Phase 5 is **DONE** only if:
- ✅ VBlank asserted at scanline 192
- ✅ VBlank cleared at scanline 0
- ⏳ VBlank IRQ fires once per frame (pending CPU core)
- ⏳ No extra or missing IRQs (pending CPU core)
- ⏳ Frame counter increments exactly once per frame (pending CPU core)
- ✅ Cadence is perfect and repeatable (timing infrastructure complete)

## Test Execution Plan

1. Load `test_vblank.bin` at address 0x0000
2. Run emulator for 10,000+ frames
3. Verify:
   - `ISR_VBLANK_COUNT` == frame count
   - `vblank_latch_count` == frame count
   - No drift or accumulation errors
4. Load `test_vblank_masked.bin` and verify:
   - `vblank_flag` still toggles
   - `ISR_VBLANK_COUNT` remains 0

## Phase 5 Pass Criteria

**Infrastructure**: ✅ COMPLETE
- VBlank flag transitions implemented and correct
- VBlank IRQ source implemented and correct
- VDP_STATUS port implemented
- IRQ controller semantics correct
- Debug visibility complete

**Full Validation**: ⚠️ Blocked on CPU core integration

The emulator is ready for Phase 5 validation as soon as a real Z80 CPU core is integrated.
