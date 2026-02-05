# Phase 5 Complete: VBlank Timing

**Date**: 2026-02-04
**Status**: ✅ INFRASTRUCTURE COMPLETE (pending CPU core for full validation)

---

## Phase 5 Goal
Implement **real VBlank state transitions** and a **real VBlank interrupt source** driven by the **Scheduler at scanline boundaries**, and validate the IRQ fires **exactly once per frame** under deterministic long-run tests.

---

## Implementation Summary

### 1. PPU VBlank State (src/devices/ppu/PPU.{h,cpp})
- Added `vblank_flag_` to PPU (moved from Scheduler)
- Added `OnScanlineStart(u16 scanline)` boundary hook:
  - Sets `vblank_flag_ = true` at scanline 192 start
  - Sets `vblank_flag_ = false` at scanline 0 start
- Added debug tracking:
  - `last_vblank_latch_scanline_` (records scanline 192)
  - `vblank_latch_count_` (counts VBlank latches)
- Added `GetVBlankFlag()` accessor for VDP_STATUS port

### 2. VBlank IRQ Source (src/console/SuperZ80Console.cpp)
- Modified `OnScanlineStart()`:
  - Calls `ppu_.OnScanlineStart(scanline)` for flag transitions
  - Raises VBlank IRQ (bit 0) at scanline 192 via `irq_.Raise(IrqBit::VBlank)`
  - Removed synthetic TIMER IRQ from Phase 4
  - Removed `synthetic_fired_this_frame_` state variable

### 3. VDP_STATUS Port (src/devices/bus/Bus.{h,cpp})
- Added port 0x10 (`VDP_STATUS`) read handler:
  - Bit 0: returns live view of `ppu_->GetVBlankFlag()`
  - Other bits: 0 for now (no sprites, no scanline compare)
  - Reading does NOT clear VBlank or IRQ bits (per spec)
- Wired PPU to Bus via `SetPPU()` in Console PowerOn

### 4. Scheduler Cleanup (src/devices/scheduler/Scheduler.{h,cpp})
- Removed `vblank_flag_` member (moved to PPU)
- Removed `IsVBlank()` accessor (VBlank is now PPU responsibility)
- Updated `GetDebugState()` to set `vblank_flag = false` (deprecated)
- Updated ring buffer recording to not track VBlank
- Removed VBlank toggle logic from `StepOneScanline()`

### 5. Debug UI Updates
- **PanelPPU.cpp**:
  - Shows `vblank_flag`, `last_vblank_latch_scanline`, `vblank_latch_count`
- **PanelIRQ.cpp**:
  - Updated title to "Phase 5: IRQ Infrastructure (VBlank)"
  - Removed synthetic fire count display
  - VBlank pending bit (bit 0) already visible

---

## Architecture Compliance

### Timing Model Compliance ✅
- VBlank flag timing follows exact specification:
  - `vblank_flag = true` at START of scanline 192
  - `vblank_flag = false` at START of scanline 0
- VBlank IRQ pending latched at scanline 192 start
- No auto-clear; pending cleared only via IRQ_ACK write
- `/INT` assertion: `(pending_bits & enable_mask) != 0`

### Ownership Rules ✅
- **Scheduler owns time**: Unchanged, still single driver of scanline progression
- **PPU owns VBlank flag**: Proper module responsibility
- **IRQController owns /INT**: Unchanged, level-sensitive interrupt controller
- **Console coordinates**: Wires PPU → IRQController at scanline boundaries

### Execution Order ✅
Per-scanline execution order (unchanged from Phase 3/4):
1. Scheduler computes cycles for this line
2. Console `OnScanlineStart()` → PPU `OnScanlineStart()` → IRQ raise
3. IRQController `PreCpuUpdate()` → recompute `/INT`
4. CPU executes for T-states
5. IRQController `PostCpuUpdate()` → recompute `/INT` after ACK
6. PPU renders visible scanline (stub)
7. DMA/APU ticks (stubs)
8. Advance scanline counter

---

## Forbidden Changes Checklist ✅
Did NOT implement or change:
- ❌ DMA engine behavior (Phase 6)
- ❌ VRAM/palette/tile/sprite logic
- ❌ Any "VRAM safe window" policies beyond vblank_flag existence
- ❌ Audio
- ❌ Any frame-based shortcuts (remains scanline-driven)

---

## Test ROM Deliverables

Created in `roms/phase5/`:
- **README.md**: Complete specification and validation requirements
- **test_vblank.asm**: Primary test ROM (VBlank IRQ enabled)
  - Configures IM 1, enables VBlank IRQ
  - ISR increments counter and ACKs
  - Main loop polls VDP_STATUS
- **test_vblank_masked.asm**: Negative test ROM (VBlank IRQ masked)
  - Leaves VBlank disabled
  - Confirms VBlank flag still toggles
  - Confirms ISR never runs

---

## Phase 5 Checklist Status

Per `docs/design/super_z80_emulator_bringup_checklist.md`:

- [x] VBlank asserted at scanline 192
- [x] VBlank cleared at scanline 0
- [ ] VBlank IRQ fires once per frame ⚠️ (blocked on CPU core)
- [ ] No extra or missing IRQs ⚠️ (blocked on CPU core)
- [ ] Frame counter increments exactly once per frame ⚠️ (blocked on CPU core)

**Pass Criteria**: VBlank IRQ cadence is perfect and repeatable.

**Current Status**: Infrastructure 100% complete. Full validation blocked on Z80 CPU core integration.

---

## What's Working

### Emulator Infrastructure ✅
1. VBlank flag transitions at exact scanlines (0 and 192)
2. VBlank IRQ pending latched at scanline 192 start
3. VDP_STATUS port returns VBlank flag in bit 0
4. IRQ_STATUS port shows VBlank pending in bit 0
5. IRQ_ENABLE/IRQ_ACK semantics correct (from Phase 4)
6. Debug UI shows all VBlank state in real-time
7. Build succeeds with no warnings

### Long-Run Determinism ✅
- Scheduler fractional accumulator maintains perfect timing (Phase 3)
- Scanline counter cycles 0→261→0 indefinitely
- VBlank latch count increments exactly once per 262 scanlines
- No drift or accumulation errors over extended runtime

---

## What's Pending

### CPU Core Integration ⚠️
- Current `Z80CpuStub` does not execute Z80 instructions
- Cannot load or run test ROMs
- Cannot detect ISR entry/exit
- Cannot validate `/INT` sampling timing
- **Blocking Full Phase 5 Validation**

### Next Steps for Full Validation
When real Z80 CPU is integrated:
1. Load `test_vblank.bin` at address 0x0000
2. Run for 10,000+ frames
3. Verify `ISR_VBLANK_COUNT` == `vblank_latch_count`
4. Verify no double IRQs or missed frames
5. Load `test_vblank_masked.bin` and verify ISR never runs

---

## Files Modified

### Core Implementation
- `src/devices/ppu/PPU.{h,cpp}` - VBlank flag and boundary hook
- `src/devices/bus/Bus.{h,cpp}` - VDP_STATUS port (0x10)
- `src/console/SuperZ80Console.{h,cpp}` - VBlank IRQ trigger, PPU wiring
- `src/devices/scheduler/Scheduler.{h,cpp}` - Removed VBlank ownership

### Debug UI
- `src/debugui/panels/PanelPPU.cpp` - VBlank visibility
- `src/debugui/panels/PanelIRQ.cpp` - Updated for Phase 5

### Documentation
- `roms/phase5/README.md` - Test ROM specification
- `roms/phase5/test_vblank.asm` - Primary test ROM source
- `roms/phase5/test_vblank_masked.asm` - Negative test ROM source

---

## Commit Message

```
Phase 5 implementation: VBlank timing

- Move VBlank flag from Scheduler to PPU (proper ownership)
- Add PPU OnScanlineStart() boundary hook for flag transitions
- Wire VBlank IRQ pending at scanline 192 start
- Add VDP_STATUS port (0x10) returning VBlank flag
- Remove synthetic IRQ from Phase 4
- Update debug UI with VBlank visibility
- Create Phase 5 test ROM specifications

Infrastructure complete; full validation blocked on CPU core integration.
```

---

## Build Status
✅ Builds successfully with no errors or warnings

## Runtime Status
✅ Emulator runs with correct VBlank timing observable in debug panels

## Ready for Phase 6?
⚠️ **No** - Phase 5 full validation requires CPU core integration first.

However, Phase 6 (DMA Engine) infrastructure can be developed in parallel since it only depends on:
- VBlank flag existence ✅
- Scanline timing ✅
- Scheduler ownership ✅

Phase 6 full validation will also be blocked on CPU core integration.

---

**Phase 5 Infrastructure: COMPLETE**
**Phase 5 Full Validation: PENDING CPU CORE**
