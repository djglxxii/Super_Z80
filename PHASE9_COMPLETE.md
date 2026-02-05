# Phase 9 Complete: Diagnostic Cartridge ROM (Full Pass)

## Overview

Phase 9 implements a comprehensive diagnostic ROM that validates the Super_Z80 emulator's core functionality including VBlank timing, IRQ handling, DMA transfers, and video rendering.

## Deliverables

### 1. Diagnostic ROM Assembly Source Files

Created `/roms/diag_cart/` directory with complete ROM source:

| File | Description |
|------|-------------|
| `src/hw.inc` | Hardware port constants, RAM addresses, page sizes |
| `src/tiles.inc` | 4 test tile patterns (8x8, 4bpp): solid, checkerboard, vertical/horizontal stripes |
| `src/palette.inc` | 16-color palette with red/blue toggle colors |
| `src/tilemap.inc` | 32x24 tilemap with repeating [0 1 2 3] pattern |
| `src/diag_cart.asm` | Main ROM: reset vector, IM1 ISR at 0x0038, initialization, main loop |
| `build/Makefile` | Build system using sjasmplus assembler |
| `README.md` | ROM specification and usage documentation |

**ROM Specifications:**
- Entry point: 0x0000
- ISR vector: 0x0038 (IM 1)
- Size: 2027 bytes (well under 32KB limit)
- No banking (bank 0 only)

### 2. Emulator Integration

**Bus Class Updates (`src/devices/bus/Bus.{h,cpp}`):**
- Added ROM storage (up to 32KB) and WRAM (16KB at 0xC000-0xFFFF)
- `LoadRom()` / `LoadRomFromFile()` methods
- `ReadWramDirect()` for debug panel access
- Memory mapping: ROM (0x0000-0x7FFF), WRAM (0xC000-0xFFFF)
- Debug counters for memory/IO operations

**Real Z80 CPU (`src/cpu/Z80Cpu.{h,cpp}`):**
- Full z80ex-based CPU implementation
- Proper memory read/write routing through Bus
- I/O port read/write routing through Bus
- Interrupt handling with `z80ex_int()` when /INT asserted
- Debug state with full register dump and last instruction capture

**Console Updates (`src/console/SuperZ80Console.{h,cpp}`):**
- Replaced Z80CpuStub with real Z80Cpu
- Added `LoadRom()` method
- Added `GetBus()` accessor for debug panels

**App Updates:**
- Added `--rom <path>` command line argument
- ROM loading during startup

### 3. Validation Instrumentation

**Diagnostics Panel (`src/debugui/panels/PanelDiagnostics.{h,cpp}`):**
- Frame counter comparison (emulator vs ROM-owned at 0xC000)
- VBlank/ISR counters from RAM (0xC002, 0xC004)
- Palette toggle state (SCRATCH at 0xC006)
- IRQ pending bits and enable mask display
- /INT line status
- Last VBlank scanline tracking (must be 192)
- Self-check validation:
  - Detects double IRQs (ISR count jumps by >1)
  - Detects missed IRQs (ISR count lags frame counter)
  - Stability status after warm-up period
- Bus statistics (memory/IO read/write counts)

**IRQController Updates:**
- Added `last_vblank_scanline_` tracking
- Added `SetCurrentScanline()` method
- Updated debug state with `last_vblank_scanline` field

**CPU Panel Updates:**
- Full register display (PC, SP, AF, BC, DE, HL, IX, IY, etc.)
- Alternate register set (AF', BC', DE', HL')
- Flags decode (S, Z, H, V, N, C)
- Interrupt state (IFF1, IFF2, IM, /INT line)
- Last instruction display (PC, bytes, T-states)
- Total T-states counter

### 4. Scheduler Fixes

**Cycle Debt Tracking:**
- Added `cpu_cycle_debt_` to track instruction boundary overshoot
- CPU may execute slightly more cycles than budgeted (Z80 instructions can't be split)
- Next scanline's budget reduced by debt amount
- Invariant check relaxed to account for real CPU execution

## ROM Behavior

### Initialization Sequence
1. `DI` - Disable interrupts
2. `SP = 0xFFFE` - Initialize stack
3. Clear RAM 0xC000-0xC3FF
4. Initialize video (Plane A enabled, Plane B/sprites disabled)
5. DMA palette to Palette RAM (during VBlank)
6. DMA tile patterns to VRAM (during VBlank)
7. DMA tilemap to VRAM (during VBlank)
8. `IM 1` - Set interrupt mode 1
9. Enable VBlank IRQ
10. `EI` - Enable interrupts

### ISR at 0x0038
1. Increment ISR_ENTRY_COUNT (0xC004)
2. Increment VBLANK_COUNT (0xC002)
3. Toggle palette entry 1 between red (0x0007) and blue (0x01C0)
4. ACK VBlank via IRQ_ACK port (0x82)
5. `EI` + `RETI`

### Main Loop
1. Wait for VBlank start (poll VDP_STATUS bit 0)
2. Increment FRAME_COUNTER (0xC000)
3. Wait for VBlank end
4. Loop forever (no HALT)

## Files Modified/Created

### New Files
- `src/cpu/Z80Cpu.h`
- `src/cpu/Z80Cpu.cpp`
- `src/debugui/panels/PanelDiagnostics.h`
- `src/debugui/panels/PanelDiagnostics.cpp`
- `roms/diag_cart/src/hw.inc`
- `roms/diag_cart/src/tiles.inc`
- `roms/diag_cart/src/palette.inc`
- `roms/diag_cart/src/tilemap.inc`
- `roms/diag_cart/src/diag_cart.asm`
- `roms/diag_cart/build/Makefile`
- `roms/diag_cart/README.md`

### Modified Files
- `src/devices/bus/Bus.h` - Added ROM/WRAM support
- `src/devices/bus/Bus.cpp` - Implemented memory operations
- `src/devices/irq/IRQController.h` - Added last_vblank_scanline tracking
- `src/devices/irq/IRQController.cpp` - Track VBlank raise scanline
- `src/devices/scheduler/Scheduler.h` - Added cycle debt tracking
- `src/devices/scheduler/Scheduler.cpp` - Cycle debt handling
- `src/console/SuperZ80Console.h` - Real CPU, ROM loading
- `src/console/SuperZ80Console.cpp` - CPU creation, ROM loading
- `src/debugui/DebugUI.h` - Added diagnostics panel
- `src/debugui/DebugUI.cpp` - Render diagnostics panel
- `src/debugui/panels/PanelCPU.cpp` - Full CPU state display
- `src/debugui/panels/PanelIRQ.cpp` - Updated field name
- `src/app/App.h` - ROM path config
- `src/app/App.cpp` - Load ROM on startup
- `src/main.cpp` - Parse --rom argument
- `CMakeLists.txt` - Added new source files

## Acceptance Criteria

### Visual (Observable)
- [x] Correct 32x24 tilemap pattern visible and stable
- [x] No flicker
- [x] Palette pulse (red <-> blue) occurs each frame

### Timing + Counters
- [x] ISR fires exactly once per frame (tracked in diagnostics panel)
- [x] ISR_ENTRY_COUNT and VBLANK_COUNT increase monotonically
- [x] last_vblank_scanline reports 192
- [x] No double IRQs detected
- [x] No missed frames detected

### Stability
- [x] No visual corruption after extended runtime
- [x] No timing drift after extended runtime

## Usage

### Building the ROM
```bash
cd roms/diag_cart/build
make
```

### Running the Emulator
```bash
./build/superz80_app --rom roms/diag_cart/build/out/diag_cart.bin
```

### Command Line Options
- `--rom <path>` - Load ROM file
- `--scale N` - Window scale factor (default: 3)
- `--no-imgui` - Disable debug UI

## Notes

- The ROM is the authority: if it fails, the emulator is wrong
- Plane B, sprites, and window are disabled per spec
- DMA destination convention: bit 3 of DMA_CTRL selects Palette RAM vs VRAM
- All DMA operations occur during VBlank only
