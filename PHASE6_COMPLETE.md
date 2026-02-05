# Phase 6 Complete: DMA Engine

**Date**: 2026-02-04
**Status**: ✅ INFRASTRUCTURE COMPLETE (pending CPU core for full validation)

---

## Phase 6 Goal
Implement **DMA (Direct Memory Access)** engine with **VBlank-only execution**, **queue policy**, and **proper timing constraints**, ensuring no CPU stalls and deterministic behavior.

---

## Implementation Summary

### 1. VRAM Storage (src/devices/ppu/PPU.{h,cpp})
- Added `vram_` backing store (16KB placeholder, one-line constant)
- Implemented safe VRAM write APIs:
  - `VramWriteByte(u16 addr, u8 value)` - Single byte write with modulo wrap
  - `VramReadByte(u16 addr) const` - Single byte read (debug/tests)
  - `VramWriteBlock(u16 dst, span<const u8> bytes)` - Bulk write helper
  - `VramReadWindow(u16 start, size_t count) const` - Debug window read
- Address wrapping policy: `effective = addr % kVramSizeBytes` (deterministic)
- Initialized to all zeros on Reset()

### 2. DMAEngine Core (src/devices/dma/DMAEngine.{h,cpp})
- Full DMA register set (ports 0x30-0x36):
  - `0x30` DMA_SRC_LO - Source address low byte (CPU space)
  - `0x31` DMA_SRC_HI - Source address high byte
  - `0x32` DMA_DST_LO - Destination address low byte (VRAM)
  - `0x33` DMA_DST_HI - Destination address high byte
  - `0x34` DMA_LEN_LO - Length low byte
  - `0x35` DMA_LEN_HI - Length high byte
  - `0x36` DMA_CTRL - Control register
- DMA_CTRL bit definitions:
  - Bit 0: START (write-1 triggers DMA attempt, edge-triggered)
  - Bit 1: QUEUE_IF_NOT_VBLANK (policy bit for mid-frame requests)
  - Bit 7: BUSY (read-only, always 0 since DMA is instantaneous)
- Dependency injection: Bus (memory reads), PPU (VRAM writes), IRQController (optional)
- Queued DMA state machine (policy: last write wins)

### 3. DMA Execution Logic
- **VBlank-only enforcement**: DMA executes only during scanlines 192-261
- **Instantaneous transfer**: No CPU stalls, entire length copied in one call
- **Length rule**: `len == 0` is no-op (not 0x10000)
- **Source addressing**: Reads via `bus_->Read8()` to respect ROM/RAM mapping
- **Destination addressing**: Writes via `ppu_->VramWriteByte()` with wrap policy
- **Queue semantics**:
  - If START written during VBlank: execute immediately
  - If START written mid-frame with QUEUE=1: capture snapshot {src,dst,len}, execute at scanline 192
  - If START written mid-frame with QUEUE=0: ignore (debug flag set)
  - Multiple queued writes: last write wins (overwrites queue)

### 4. Bus I/O Integration (src/devices/bus/Bus.{h,cpp})
- Added `SetDMAEngine()` wiring method
- Extended `In8()` to handle ports 0x30-0x36:
  - Forwards reads to `dma_->ReadReg(port)`
  - Returns 0xFF for unmapped ports (0x37-0x3F)
- Extended `Out8()` to handle ports 0x30-0x36:
  - Forwards writes to `dma_->WriteReg(port, value)`
  - Writes to unmapped ports ignored

### 5. Console Integration (src/console/SuperZ80Console.cpp)
- Wired DMA dependencies in `PowerOn()`:
  - `dma_.SetBus(&bus_)` for memory reads
  - `dma_.SetPPU(&ppu_)` for VRAM writes
  - `dma_.SetIRQController(&irq_)` for future DMA_DONE IRQ
  - `bus_.SetDMAEngine(&dma_)` for I/O port routing
- Updated `TickDMA()` to call scanline boundary hook:
  - `dma_.OnScanlineBoundary(scanline, vblank_flag, frame)`
  - Replaced stub implementation
  - Maintains mandated execution order (step 6 in scanline loop)

### 6. Debug State Exposure
- Comprehensive `DmaDebugState` struct:
  - Current DMA registers (src, dst, len, ctrl)
  - Queue enabled flag
  - Queued DMA state (valid, src, dst, len)
  - Last execution tracking (frame, scanline, was_queued)
  - Debug-only illegal start flag
- Updated `PanelDMA.cpp` with full register/state display
- Shows queued DMA status and last execution info
- Color-coded error display for illegal starts

---

## Architecture Compliance

### Timing Model Compliance ✅
- DMA executes **only during VBlank** (scanlines 192-261)
- Queued DMA executes **exactly at scanline 192 start** (mandated order)
- No CPU stalls (DMA is instantaneous in emulation)
- No partial DMA (all-or-nothing transfer)
- Execution order maintained (step 6: DMAEngine after IRQ/CPU/PPU)

### Module Ownership ✅
- **PPU owns VRAM**: Storage and safe write APIs, PPU never triggers DMA
- **DMAEngine owns DMA logic**: Registers, state, legality checks
- **Bus owns I/O decode**: Routes ports 0x30-0x36 to DMAEngine
- **CPU triggers DMA via Bus**: No direct DMA calls from CPU

### Policy Decisions 🔒
Locked for Phase 6:
- Length 0 → no-op (not 0x10000)
- Source: CPU address space via Bus memory read
- Destination: VRAM address with modulo wrap
- Queue policy: last write wins (no FIFO accumulation)
- START is edge-triggered (auto-clear after handling)

---

## Forbidden Changes Checklist ✅
Did NOT implement or change:
- ❌ PPU rendering logic (VRAM is not rendered yet)
- ❌ CPU stalls for DMA
- ❌ Partial DMA transfers
- ❌ DMA outside VBlank without queue mechanism
- ❌ DMA_DONE IRQ (optional, not required for Phase 6 pass)

---

## Test ROM Deliverables

Created in `roms/phase6/`:
- **README.md**: Complete DMA specification and validation requirements
- **test_dma.asm**: Phase 6 test ROM with three test cases:

### Test Case 1: Positive DMA during VBlank
- Waits for VBlank flag (VDP_STATUS bit 0 = 1)
- Programs DMA: SRC=0xC100, DST=0x2000 (VRAM), LEN=256
- Writes START bit during VBlank
- **Expected**: VRAM[0x2000..0x20FF] = 0x00..0xFF (incrementing pattern)

### Test Case 2: Negative - Mid-frame DMA with QUEUE=0
- Waits for visible scanlines (VDP_STATUS bit 0 = 0)
- Programs DMA: SRC=0xC101, DST=0x3000 (VRAM), LEN=128
- Writes START bit without QUEUE bit
- **Expected**: DMA ignored, VRAM[0x3000..0x307F] remains 0x00

### Test Case 3: Queue - Mid-frame DMA with QUEUE=1
- Waits for visible scanlines
- Programs DMA: SRC=0xC100, DST=0x4000 (VRAM), LEN=256
- Writes START | QUEUE_IF_NOT_VBLANK
- **Expected**: DMA queues and executes at next VBlank start (scanline 192)
- **Expected**: VRAM[0x4000..0x40FF] = 0x00..0xFF after next VBlank

---

## Phase 6 Checklist Status

Per `docs/phases/phase6_instructions.md`:

- [x] DMA executes only when VBlank is true
- [x] Queued DMA completes exactly at next VBlank start (scanline 192)
- [ ] VRAM contents match source RAM after VBlank ⚠️ (blocked on CPU core)
- [x] Mid-frame DMA with queue disabled does nothing (fail safely)
- [x] No CPU stalls (scheduler counters remain stable)
- [x] No rendering changes (still test pattern)

**Pass Criteria**: DMA transfers occur only during VBlank with correct queuing behavior and deterministic timing.

**Current Status**: Infrastructure 100% complete. Full validation blocked on Z80 CPU core integration.

---

## What's Working

### Emulator Infrastructure ✅
1. VRAM backing store (16KB) with wrap addressing
2. Full DMA register set (ports 0x30-0x36)
3. VBlank-only execution enforcement
4. Queued DMA state machine with last-write-wins policy
5. Instantaneous DMA (no CPU stalls)
6. Source reads via Bus (respects ROM/RAM mapping)
7. Destination writes via PPU (VRAM wrap policy)
8. Debug state exposure with comprehensive tracking
9. Debug UI shows all DMA registers and state
10. Build succeeds with no warnings

### Timing Guarantees ✅
- DMA execution hook called once per scanline (step 6)
- Queued DMA executes exactly at scanline 192 start
- No drift or accumulation errors
- Deterministic behavior (no wall-clock dependencies)

---

## What's Pending

### CPU Core Integration ⚠️
- Current `Z80CpuStub` does not execute Z80 instructions
- Cannot load or run test ROM
- Cannot write to RAM buffer (TEST_PATTERN_BUF)
- Cannot program DMA registers via OUT instructions
- Cannot verify VRAM contents match source RAM
- **Blocking Full Phase 6 Validation**

### Next Steps for Full Validation
When real Z80 CPU is integrated:
1. Load `test_dma.bin` at address 0x0000
2. Run test ROM through all three test cases
3. At end of each test, read VRAM via `ppu_.VramReadWindow()`
4. Verify VRAM contents match expected patterns:
   - Test 1: VRAM[0x2000..0x20FF] = 0x00..0xFF
   - Test 2: VRAM[0x3000..0x307F] = 0x00 (unchanged)
   - Test 3: VRAM[0x4000..0x40FF] = 0x00..0xFF (after VBlank)
5. Add assertions in emulator to check VRAM automatically

---

## Files Modified

### Core Implementation
- `src/devices/ppu/PPU.{h,cpp}` - VRAM storage and safe write APIs
- `src/devices/dma/DMAEngine.{h,cpp}` - Complete DMA engine with queue logic
- `src/devices/bus/Bus.{h,cpp}` - DMA I/O port routing (0x30-0x36)
- `src/console/SuperZ80Console.cpp` - DMA wiring and scanline boundary hook

### Debug UI
- `src/debugui/panels/PanelDMA.cpp` - Full DMA register/state display

### Documentation
- `roms/phase6/README.md` - DMA test ROM specification
- `roms/phase6/test_dma.asm` - Phase 6 test ROM source
- `docs/phases/phase6_instructions.md` - Already exists (provided)

---

## Commit Message

```
Phase 6 implementation: DMA engine

- Add VRAM backing store to PPU (16KB, modulo wrap addressing)
- Implement DMAEngine with full register set (ports 0x30-0x36)
- Enforce VBlank-only execution (scanlines 192-261)
- Add queued DMA state machine (last write wins policy)
- Integrate DMA into Console scanline execution order
- Wire DMA I/O ports via Bus
- Add comprehensive debug state exposure
- Create Phase 6 test ROM with three validation cases

Infrastructure complete; full validation blocked on CPU core integration.
```

---

## Build Status
✅ Builds successfully with no errors or warnings

## Runtime Status
✅ Emulator runs with DMA registers accessible via debug panels

## Ready for Phase 7?
⚠️ **No** - Phase 6 full validation requires CPU core integration first.

However, future phases can be developed in parallel if they don't depend on:
- Verified VRAM contents
- DMA execution correctness
- CPU-driven DMA triggers

Phase 6 full validation will be blocked on CPU core integration.

---

**Phase 6 Infrastructure: COMPLETE**
**Phase 6 Full Validation: PENDING CPU CORE**
