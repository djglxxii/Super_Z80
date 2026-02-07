# Super_Z80 Emulator Bring-Up Checklist (Authoritative)

**Status:** Required
**Audience:** Emulator implementation and validation
**Purpose:** Ensure each subsystem is proven correct before proceeding to the next.
**Rule:** Do not advance to a later phase until all checks in the current phase pass.

If a later phase fails, return to the earliest failed phase.

---

## Phase 0 — Project Scaffolding (No Emulation Yet)

**Goal:** Ensure the project is structurally correct before any logic is written.

### Checklist

* [ ] Repository structure matches architecture document
* [ ] CMake builds a runnable executable
* [ ] SDL2 window opens at 256×192 (scaled as needed)
* [ ] Main loop exists but performs no emulation
* [ ] Logging and assertion framework in place
* [ ] Debug UI shell (empty panels) loads

**Pass Criteria:**
Executable runs, opens window, exits cleanly.

---

## Phase 1 — CPU in Isolation

**Goal:** Prove Z80 execution is correct *without* video, audio, or IRQs.

### Checklist

* [ ] Z80 core integrated and callable
* [ ] CPU reset vector executes
* [ ] Memory reads/writes routed through Bus
* [ ] Test ROM executes deterministic instruction sequence
* [ ] PC, SP, registers behave correctly
* [ ] Instruction cycle counts consumed correctly

**Forbidden in this phase:**

* IRQs
* DMA
* Video
* Audio

**Pass Criteria:**
CPU executes a known loop indefinitely with correct register evolution.

---

## Phase 2 — Bus & Memory Map Validation

**Goal:** Ensure CPU-visible memory behaves exactly as specified.

### Checklist

* [ ] Work RAM mapped at `0xC000–0xFFFF`
* [ ] ROM mapped at `0x0000–0x7FFF`
* [ ] Unmapped memory returns open-bus (`0xFF`)
* [ ] I/O reads/writes dispatch correctly
* [ ] Mapper reset behavior enforced

**Pass Criteria:**
CPU reads/writes hit the correct backing storage every time.

---

## Phase 3 — Scheduler & Scanline Timing

**Goal:** Lock time progression before adding hardware behavior.

### Checklist

* [ ] Scheduler advances **one scanline at a time**
* [ ] Scanline counter cycles 0 → 261 → 0
* [ ] Fractional CPU cycle accumulator implemented
* [ ] Average CPU frequency matches timing model
* [ ] Frame pacing stable over time

**Pass Criteria:**
Scanline counter, frame counter, and CPU cycles remain stable over minutes.

---

## Phase 4 — IRQ Infrastructure (No Video Yet)

**Goal:** Prove interrupt delivery and acknowledgment work.

### Checklist

* [ ] IRQController maintains pending bits
* [ ] IRQ_ENABLE masks correctly
* [ ] `/INT` asserted only when appropriate
* [ ] IM 1 vector jumps to `0x0038`
* [ ] EI delay honored
* [ ] IRQ_ACK clears pending bits

**Pass Criteria:**
CPU enters ISR exactly when expected and returns cleanly.

---

## Phase 5 — VBlank Timing (Still No Rendering)

**Goal:** Validate video timing signals without drawing anything.

### Checklist

* [ ] VBlank asserted at scanline 192
* [ ] VBlank cleared at scanline 0
* [ ] VBlank IRQ fires once per frame
* [ ] No extra or missing IRQs
* [ ] Frame counter increments exactly once per frame

**Pass Criteria:**
VBlank IRQ cadence is perfect and repeatable.

---

## Phase 6 — DMA Engine

**Goal:** Prove RAM → VRAM transfers are correct and legal.

### Checklist

* [ ] DMA registers writable via I/O
* [ ] DMA only executes during VBlank
* [ ] DMA queued correctly when triggered early
* [ ] DMA never affects visible scanlines
* [ ] DMA_DONE IRQ raised (if implemented)

**Negative Tests (Must Fail Safely):**

* [ ] DMA triggered mid-frame does not corrupt display
* [ ] DMA outside VBlank obeys queue/ignore policy

**Pass Criteria:**
VRAM contents exactly match source RAM after VBlank.

---

## Phase 7 — PPU Bring-Up (Plane A Only)

**Goal:** Prove tile rendering works.

### Checklist

* [ ] VRAM pattern data decodes correctly
* [ ] Tilemap lookup works
* [ ] Plane A scroll registers apply
* [ ] Palette indexing correct
* [ ] Scanline renderer produces stable output

**Pass Criteria:**
Static tilemap renders correctly and consistently.

---

## Phase 8 — Palette & Mid-Frame Behavior

**Goal:** Validate palette handling and timing.

### Checklist

* [ ] Palette DMA works
* [ ] Palette changes visible only at correct boundary
* [ ] Mid-frame palette changes affect next scanline (not earlier)
* [ ] No tearing or partial updates

**Pass Criteria:**
Palette-driven visual effects are stable and deterministic.

---

## Phase 9 — Diagnostic ROM (Phase 1)

**Goal:** Pass the diagnostic cartridge ROM spec.

### Checklist

* [ ] Diagnostic ROM boots
* [ ] Screen displays expected tile pattern
* [ ] Palette pulses once per frame
* [ ] ISR count matches frame count
* [ ] No corruption after extended runtime

**Pass Criteria:**
All success criteria in `super_z80_diagnostic_cartridge_rom_spec.md` are met.

---

## Phase 10 — Plane B (Parallax)

**Goal:** Enable second background plane.

### Checklist

* [ ] Plane B base registers respected
* [ ] Independent scroll works
* [ ] Priority rules honored
* [ ] No interference with Plane A

**Pass Criteria:**
Dual-plane rendering behaves as expected.

---

## Phase 11 — Sprites

**Goal:** Enable sprite system.

### Checklist

* [ ] Sprite attribute table parsed correctly
* [ ] Per-scanline sprite limit enforced (16)
* [ ] Overflow flag/IRQ works
* [ ] Priority vs planes correct

**Pass Criteria:**
Sprites render correctly without flicker beyond hardware limits.

---

## Phase 12 — Audio Bring-Up (PSG + YM2151)

**Goal:** Enable premium audio while preserving timing determinism.

### Checklist

* [ ] PSG produces stable tones and noise
* [ ] YM2151 produces correct FM output
* [ ] Audio mixing stable and balanced
* [ ] No crackle or drift over long runs
* [ ] Audio remains synchronized with video timing

**Explicitly excluded:**

* PCM playback
* Sample streaming
* Audio-driven IRQs

### Pass Criteria

Audio remains stable, synchronized, and deterministic over extended runtime.

---

## Hard Rules

* ❌ Do not skip phases
* ❌ Do not “work around” failing phases
* ❌ Do not blame ROMs for emulator bugs
* ✅ Fix issues at the phase they appear

---

## Why This Checklist Exists

Most emulator failures come from:

* Building too much at once
* Assuming subsystems work
* Debugging symptoms instead of causes

This checklist prevents all three.

---

