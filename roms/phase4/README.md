# Phase 4 IRQ Test ROM

## Overview
This directory contains the test ROM for Phase 4: IRQ Infrastructure validation.

## Files
- `test_irq.asm` - Z80 assembly source for IRQ test ROM

## ROM Requirements (Per Phase 4 Specification)
The test ROM must:
1. Configure IM 1 (interrupt mode 1, vectors to 0x0038)
2. Enable TIMER interrupt source by writing to IRQ_ENABLE port (0x81) with bit 1 set
3. Provide an ISR at address 0x0038 that:
   - Reads IRQ_STATUS (0x80) for debugging (optional, does not clear)
   - ACKs TIMER pending by writing to IRQ_ACK (0x82) with bit 1 set
   - Increments a counter in Work RAM (0xC000) to prove execution
   - Returns with RETI

## Assembly and Loading
To assemble this ROM (once z80asm or similar is available):
```bash
z80asm -o test_irq.bin test_irq.asm
```

## Current Status (Phase 4)
- **IRQ Infrastructure**: ✅ COMPLETE
  - IRQController with latched pending bits, enable mask, /INT drive
  - Bus I/O ports 0x80/0x81/0x82 with correct semantics
  - Synthetic TIMER IRQ raised at scanline 10 each frame
  - PreCpuUpdate/PostCpuUpdate correctly integrated in scanline loop
  - CPU /INT line wired to IRQController
  - Debug visibility (PanelIRQ shows pending, enable, /INT state, counters)

- **CPU Core**: ⚠️ STUB
  - Z80CpuStub accepts /INT line but does not execute instructions
  - Full validation requires real Z80 CPU core integration
  - ISR entry detection needs PC tracking (not yet implemented)

## Validation Plan
When real Z80 CPU is integrated:
1. Load test_irq.bin into Bus memory at 0x0000
2. Run emulator for several frames
3. Verify ISR entry count matches synthetic fire count (once per frame)
4. Verify /INT asserts when TIMER pending & enabled
5. Verify /INT drops immediately after ISR writes to IRQ_ACK
6. Verify no interrupts occur when TIMER is disabled via IRQ_ENABLE

## Phase 4 Pass Criteria
All infrastructure is in place for Phase 4 PASS. Pending items:
- Real Z80 CPU core integration (for instruction execution)
- PC tracking for ISR entry detection (CPU debug hook)
- ROM loading infrastructure (cartridge/bus memory)
- Full end-to-end test execution

The IRQ controller, I/O ports, timing, and debug visibility are fully functional.
