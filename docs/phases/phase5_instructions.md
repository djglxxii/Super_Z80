You are a coding agent implementing **Super_Z80 emulator Bring-Up Phase 5: VBlank Timing (Still No Rendering)**.

Authoritative references (must follow exactly):

* `super_z80_emulator_bringup_checklist.md` (Phase 5 checklist + pass criteria)
* `super_z80_emulation_timing_model.md` (VBlank/IRQ timing rules)
* `super_z80_emulator_architecture.md` (**Scheduler owns time**, **IRQController owns /INT**)
* `super_z80_io_register_map_skeleton.md` (I/O ports: `VDP_STATUS`, `IRQ_STATUS/ENABLE/ACK` semantics, bit mappings)
* `super_z80_project_glossary.md` (terminology)

Strict scope (hard constraints):

* NO DMA
* NO PPU rendering logic (keep Phase 0 test-pattern path only)
* NO APU/audio
* Scheduler/scanline timing from Phase 3 remains active and unchanged
* IRQ infra from Phase 4 remains; Phase 5 **replaces synthetic IRQ trigger** with real VBlank source
* Do not implement tiles/sprites/palette/VRAM behavior (only VBlank state + status bits)

Goal:
Implement **real VBlank state transitions** and a **real VBlank interrupt source** driven by the **Scheduler at scanline boundaries**, and validate the IRQ fires **exactly once per frame** under deterministic long-run tests.

---

## 1) Exact timing rules (do not deviate)

### Scanline boundary definition (Phase 5)

“Start of scanline N” means the moment the Scheduler advances to scanline N and calls device boundary hooks **before** executing that scanline’s CPU cycles.

### VBlank flag timing (must be exact)

* `vblank_flag = true` at the **START** of scanline **192**
* `vblank_flag = false` at the **START** of scanline **0**
* Therefore `vblank_flag` is true only during scanlines **192–261** (inclusive), and false during **0–191**.

### VBlank IRQ source timing (must be exact)

* At the **START** of scanline **192**, latch the **VBLANK pending** bit in `IRQController` (bit mapping per skeleton: **IRQ_STATUS bit 0**).
* Do **not** generate VBlank pending at any other time.
* Pending is **level-latched**: it stays set until software clears it via `IRQ_ACK` (write-1-to-clear).
* `/INT` assertion is owned by `IRQController` and must obey:

    * `/INT asserted` iff `(pending_bits & enable_mask) != 0`
    * `/INT remains asserted` until the relevant pending bit is cleared by software (no auto-clear).

---

## 2) Required I/O wiring (ports and semantics)

### VDP status register

* Port `0x10` — `VDP_STATUS` (R)

    * Must reflect **VBlank state** (1 when `vblank_flag` is true; 0 otherwise).
    * Other bits may be 0 for now (no sprites, no scanline compare, etc.).
    * Reading `VDP_STATUS` must **not** clear VBlank or any IRQ bits (clearing is via `IRQ_ACK` only).

> Implementation note: treat `VDP_STATUS.VBLANK` as a *live view* of `vblank_flag`, not a separate latched copy.

### IRQ controller ports (already present from Phase 4; confirm behavior)

* Port `0x80` — `IRQ_STATUS` (R): returns latched pending bits

    * Bit 0: VBLANK pending
    * Other bits may exist but are out-of-scope; keep existing behavior
* Port `0x81` — `IRQ_ENABLE` (R/W): mask controlling which pending bits may assert `/INT`
* Port `0x82` — `IRQ_ACK` (W): **write-1-to-clear** for the corresponding pending bits

    * Example: writing `0b0000_0001` clears VBLANK pending (bit 0)

Hard rule: **VBlank flag** (VDP_STATUS) and **VBlank pending** (IRQ_STATUS bit 0) are related but not identical:

* `vblank_flag` toggles at scanlines 0 and 192 regardless of masking
* `VBLANK pending` is set at scanline 192 start and cleared only by `IRQ_ACK`

---

## 3) Ownership + scheduling integration (architecture compliance)

### Scheduler owns time

Do not let CPU, VDP, or IRQController advance scanlines. The **Scheduler** must remain the single driver of scanline progression (Phase 3 behavior unchanged).

### Required boundary hook

Add/confirm an explicit device boundary callback invoked by the Scheduler:

**Interface skeleton (example names; adapt to your codebase conventions):**

* `class ISchedulerSink { virtual void OnScanlineStart(uint16_t scanline) = 0; };`
* Scheduler calls `OnScanlineStart(current_scanline)` exactly once per scanline, at the start boundary.

**Console/Motherboard wiring:**

* Scheduler calls into Motherboard (or Bus-level “devices” hub), which then calls:

    * VDP (or VideoTiming module) for `vblank_flag` transitions
    * IRQController for “latch pending sources” events
* IRQController updates `/INT` output signal after pending/enable changes.

### Ordering rule (determinism)

At the start-of-scanline boundary:

1. Update scanline counter to N
2. Call `OnScanlineStart(N)` (devices latch VBlank/IRQs here)
3. Then run CPU cycles for scanline N (Phase 3 cycle budgeting unchanged)

This ordering ensures “start of scanline 192” occurs immediately after finishing scanline 191 CPU time.

---

## 4) VBlank implementation plan (no rendering changes)

### New/confirmed state

* Store `bool vblank_flag` in the **video timing / VDP stub module** (not in Scheduler, not in CPU).
* Expose it to:

    * `VDP_STATUS` read path (port 0x10)
    * Debug UI
* Maintain (optional) debug-only tracking:

    * `uint16_t last_vblank_latch_scanline` (record scanline when VBlank pending was set)
    * `uint64_t vblank_latch_count` (counts number of times the source latched per run)

### OnScanlineStart behavior (exact logic)

On `OnScanlineStart(scanline)`:

* If `scanline == 192`:

    * `vblank_flag = true`
    * `IRQController.SetPending(IRQBit::VBlank)` (bit 0)
    * `last_vblank_latch_scanline = 192` (optional debug)
* If `scanline == 0`:

    * `vblank_flag = false`
* No other scanlines modify `vblank_flag` or generate VBlank pending.

Do not auto-ack. Do not “edge detect” in a way that could skip latching if the bit is already set—`SetPending` can be idempotent (OR-in), but it must be called only at scanline 192 start.

---

## 5) Test ROM requirements (described only; do not fully code)

Create a minimal Phase 5 validation ROM (or extend existing bring-up ROM) with these behaviors:

### Setup

* `DI`
* Initialize `SP` near top of RAM (e.g., `0xFFFE`)
* Clear / init counters in RAM:

    * `ISR_VBLANK_COUNT` (incremented only in ISR)
    * `FRAME_COUNT_MAIN` (incremented once per frame in main loop, using either ISR count or polling VBlank transitions)
    * Optionally: `LAST_VDP_STATUS` for transition detection
* Set interrupt mode: `IM 1`
* Enable VBlank IRQ source:

    * Write to `IRQ_ENABLE` (port `0x81`) setting bit 0 = 1, other bits 0
* `EI` (and ensure your CPU core honors EI delay; already a Phase 4 requirement)

### ISR at `0x0038` (IM 1 vector)

On entry:

1. Increment `ISR_VBLANK_COUNT` in RAM
2. Acknowledge VBlank pending:

    * Write `0b0000_0001` to `IRQ_ACK` (port `0x82`) (write-1-to-clear bit 0)
3. `RETI`

### Main loop (two acceptable options)

Option A (ISR-driven):

* Busy-loop forever; periodically copy `ISR_VBLANK_COUNT` to `FRAME_COUNT_MAIN` (or increment `FRAME_COUNT_MAIN` when it changes)

Option B (polling VBlank transition):

* Read `VDP_STATUS` (port `0x10`) and detect the 0→1 transition of VBlank flag
* On each detected rising edge, increment `FRAME_COUNT_MAIN`

Hard rule for test: do not use `HALT` (avoid masking timing bugs).

### Negative test (masking)

A second ROM mode (or build-time switch) that:

* Leaves VBlank masked (`IRQ_ENABLE bit 0 = 0`)
* Still polls `VDP_STATUS` to confirm VBlank toggles correctly
* Confirms `ISR_VBLANK_COUNT` stays at 0 indefinitely

---

## 6) Deterministic validation requirements (long-run, no drift)

Add an automated headless test (or debug-mode runtime assertion harness) that runs for a long duration (e.g., tens of thousands of frames) and checks:

### Assertions (must hold)

1. `vblank_flag` transitions:

    * Becomes true exactly at scanline 192 start
    * Becomes false exactly at scanline 0 start
2. With VBlank enabled (IRQ_ENABLE bit 0 = 1):

    * ISR entry count increments **exactly once per emulated frame**
    * No double IRQs, no missed frames
3. With VBlank masked (IRQ_ENABLE bit 0 = 0):

    * `vblank_flag` still toggles correctly
    * ISR never runs (`ISR_VBLANK_COUNT == 0`)
4. IRQ latching semantics:

    * VBlank pending (IRQ_STATUS bit 0) is set at scanline 192 start
    * `/INT` asserts only when enabled and pending
    * `/INT` remains asserted until `IRQ_ACK` clears the pending bit
5. No unrelated subsystem changes:

    * Rendering path remains Phase 0 test pattern only
    * No DMA/APU introduced
    * Scheduler scanline timing unchanged

### Pass/fail mapping to Phase 5 checklist

Phase 5 is “DONE” only if:

* VBlank asserted at scanline 192
* VBlank cleared at scanline 0
* VBlank IRQ fires once per frame
* No extra or missing IRQs
* Frame counter increments exactly once per frame
* Cadence is perfect and repeatable over long runtime

---

## 7) Debug visibility (required panel fields)

Expose the following in the debug UI (or logging overlay) so Phase 5 can be inspected live:

Required:

* Current scanline (0–261)
* `vblank_flag`
* IRQ pending bits (raw `IRQ_STATUS`)
* IRQ enable mask (`IRQ_ENABLE`)
* `/INT` asserted state (as seen by CPU input line)
* ISR entry counter (from emulator-side trace and/or ROM RAM watch)

Optional but recommended:

* `last_vblank_latch_scanline`
* “VBlank latched this frame” boolean (edge marker cleared at scanline 0)

Do not add emulator-only I/O ports for debug unless your project explicitly allows it; prefer internal debug UI state.

---

## 8) Non-goals / forbidden changes checklist (enforce strictly)

Do NOT implement or change:

* DMA engine behavior (Phase 6)
* VRAM/palette/tile/sprite logic
* Any “VRAM safe window” policies beyond `vblank_flag` existence
* Audio
* Any frame-based shortcuts (must remain scanline-driven)

---

## Deliverable expectation

Produce only:

* Implementation instructions
* Minimal interface skeletons / method signatures (no full source)
* Test ROM behavior description (no full ROM code)
* Explicit pass/fail criteria and what to log/observe
