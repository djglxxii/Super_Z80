# Phase 2 Bus Validation ROMs (Behavior Only)

This document describes the deterministic ROM behaviors required for Bring-Up Phase 2.
No binary or full assembly is provided here by design.

## General Conventions

- Write results to Work RAM at `0xC000-0xC0FF`.
- End each test in a tight infinite loop at a known label/PC so the host can detect completion.
- Use a signature word like `0xBEEF` (pass) or `0xDEAD` (fail) in RAM.

## ROM #1: RAM_R/W_TEST

- For N bytes (e.g., 256):
  - Write `0xA5` to `0xC000 + i`
  - Read back; on mismatch write FAIL code to `0xC0F0` and loop
- Repeat with `0x5A`
- On success write PASS code to `0xC0F0` and loop

## ROM #2: OPEN_BUS_READ_TEST

- Read from:
  - `0x8000`, `0x9000`, `0xBFFF` (Phase 2: unmapped; expect `0xFF`)
  - `0x4000` (ROM region; should NOT be open-bus)
  - `0x7FFF` (ROM last byte)
- Store read results at `0xC100+`

## ROM #3: UNMAPPED_WRITE_IGNORED_TEST

- Write to `0x8000` and `0x9000`, then read back
- Reads must still return `0xFF`
- Record results in Work RAM

## ROM #4: IO_STUB_TEST

- Execute `IN A,(0x10)` and `IN A,(0x00)`; store results in Work RAM
- Execute `OUT (0x10),A` and `OUT (0x00),A` with known values
- Expected: all IN results `0xFF`, OUT has no effect

## ROM #5: RESET_BANK0_TEST

- Include identifiable data at `0x0000`
- Host resets cartridge and CPU, steps first fetches, and verifies reads at `0x0000`
- Requirement: bank 0 mapped at reset vector

## Host Harness Outline (Phase 2)

- Load ROM into Cartridge
- Reset: `Cartridge.Reset()`, `Bus.Reset()`, `CPU.Reset()`
- Step instruction-by-instruction until:
  - PC equals known terminal loop address for K consecutive steps, or
  - max instruction budget exceeded (fail)
- Inspect Work RAM for PASS/FAIL markers
- Assert Bus debug invariants:
  - `lastAccess` updated on every access
  - Target tags correct (ROM/WorkRAM/OpenBus/IO)
  - Counter expectations per ROM
  - Deterministic counters across runs
