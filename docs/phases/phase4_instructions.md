You are a coding agent implementing **Super_Z80 Bring-Up Phase 4: IRQ Infrastructure (No Video/Audio/DMA)**.

Authoritative constraints (must comply):

* I/O IRQ ports and semantics are defined in `super_z80_io_register_map_skeleton.md`.
* IRQs are **level-sensitive**, **latched**, and must not auto-clear; ISR must ACK via I/O.
* Timing boundaries are **scanline-based**; sources may assert at **start of scanline** boundaries.
* Phase 4 success criteria are defined in `super_z80_emulator_bringup_checklist.md`.
* Architecture execution order is “locked”; see `super_z80_emulator_architecture.md`.

Important note about call order (resolve explicitly):

* The architecture doc states per-scanline order includes **CPU executes**, then **IRQController updates /INT**.
* This Phase 4 task request asks for “Scheduler tick → IRQController update → CPU step”.
* Implement a compliant hybrid:

    1. At **scanline start**, run a **scanline-start IRQ event hook** (latch synthetic pending bits) and then **compute /INT level before CPU runs** for that scanline (so the CPU can take the interrupt during that scanline’s execution window).
    2. After the CPU finishes its allocated cycles for that scanline, call **IRQController::PostCpuUpdate()** to immediately reflect any same-scanline ACK/ENABLE writes (so /INT can drop “immediately (same scanline step)” as required).
* This preserves scanline determinism and gives correct “interrupt visible at boundary” behavior without letting devices own time.

======================================================================
PHASE 4 IMPLEMENTATION GOAL
Implement IRQ infrastructure end-to-end:

* IRQController with latched pending bits, enable mask, and /INT drive.
* Bus I/O ports 0x80/0x81/0x82 with precise semantics.
* Synthetic “software-driven” IRQ source raised deterministically at a chosen scanline boundary.
* Test ROM proves IM 1 delivery to 0x0038; ISR ACKs via IRQ_ACK and returns.
* Debug visibility + hard assertions.

Strict scope:

* NO DMA behavior (ports remain stubbed/0xFF/ignored).
* NO APU.
* NO new PPU rendering logic.
* NO VBlank IRQ generation (Phase 5).
* Scheduler/scanline timing (Phase 3) remains active and unchanged in cadence; only add the scanline-start hook described above.

======================================================================
MODULE OWNERSHIP + INTERFACES (SKELETONS ONLY; DO NOT WRITE FULL CODE)

1. IRQController (new module; owned by Console/Motherboard; CPU only observes /INT line)
   Responsibilities:

* Maintain latched pending bits (8-bit).
* Maintain enable mask (8-bit).
* Compute /INT asserted = ((pending & enable) != 0).
* Level-sensitive: remains asserted until relevant pending bit cleared via IRQ_ACK.
* No auto-clear on read or on interrupt acknowledge cycle.
* Expose read-only debug state.

Suggested interface skeleton:

```cpp
// irq_controller.h
#pragma once
#include <cstdint>

enum class IrqBit : uint8_t {
  VBlank      = 1 << 0,
  Timer       = 1 << 1,
  Scanline    = 1 << 2,
  SprOverflow = 1 << 3,
  DmaDone     = 1 << 4, // optional; keep defined but unused in Phase 4
};

struct IrqDebugState {
  uint16_t scanline;            // provided by Scheduler for display
  uint8_t pending;              // latched
  uint8_t enable;               // mask
  bool int_line_asserted;       // derived
  uint64_t isr_entry_count;      // maintained by test harness observation
  uint64_t synthetic_fire_count; // maintained by scheduler trigger
};

class IRQController {
public:
  void Reset(); // clears pending, enable=0, /INT deasserted

  // Latch/set pending bits (OR-in), regardless of enable mask.
  void Raise(uint8_t pending_mask);

  // Write-1-to-clear latched pending bits.
  void Ack(uint8_t w1c_mask);

  // R/W enable
  uint8_t ReadEnable() const;
  void WriteEnable(uint8_t mask);

  // R-only status (no side effects)
  uint8_t ReadStatus() const;

  // Called at scanline start AFTER any scanline-start synthetic raises.
  void PreCpuUpdate();   // recompute /INT (level) from pending & enable

  // Called after CPU ran this scanline’s cycles and after any I/O writes occurred.
  void PostCpuUpdate();  // recompute /INT again so ACK drops immediately

  bool IntLineAsserted() const;

  IrqDebugState GetDebug(uint16_t current_scanline) const;

private:
  uint8_t pending_ = 0;
  uint8_t enable_  = 0;
  bool int_line_   = false;

  void RecomputeIntLine(); // int_line_ = ((pending_ & enable_) != 0)
};
```

Hard rules to enforce inside IRQController:

* Pending bits are latched and only cleared by Ack(w1c).
* /INT is purely derived from (pending & enable).
* /INT must drop immediately when (pending & enable) becomes 0, without waiting a frame/scanline (achieve via PostCpuUpdate and also immediate recompute in Ack/WriteEnable if you choose).

2. Bus I/O dispatch (existing Bus; add ports)
   Implement these ports exactly:

* `0x80 IRQ_STATUS (R)`:

    * Returns **latched pending bits**.
    * **No auto-clear** on read.

* `0x81 IRQ_ENABLE (R/W)`:

    * Read returns current enable mask.
    * Write sets enable mask (8-bit).

* `0x82 IRQ_ACK (W)`:

    * Write-1-to-clear pending bits: for each bit that is 1 in the written value, clear that pending bit.
    * Writing 0 has no effect.
    * No other side effects.

* All other `0x83–0x8F` remain stubbed for Phase 4:

    * Reads return `0xFF`, writes ignored (consistent with skeleton default for unmapped/reserved).

Bus I/O interface sketch (do not implement full bus, just show hook points):

```cpp
// bus_io.h
uint8_t Bus::IoRead(uint16_t port) {
  const uint8_t p = port & 0xFF;
  switch (p) {
    case 0x80: return irq_.ReadStatus();
    case 0x81: return irq_.ReadEnable();
    default:   return 0xFF;
  }
}

void Bus::IoWrite(uint16_t port, uint8_t value) {
  const uint8_t p = port & 0xFF;
  switch (p) {
    case 0x81: irq_.WriteEnable(value); irq_.PostCpuUpdate(); return;
    case 0x82: irq_.Ack(value);         irq_.PostCpuUpdate(); return;
    default:   return; // ignored
  }
}
```

Note: calling PostCpuUpdate() inside the write handlers is allowed and recommended to satisfy “INT drops immediately (same scanline step)” even if the formal “PostCpuUpdate” is also called once after CPU runs.

3. CPU IM 1 integration (plumbing only)

* CPU core remains unchanged.
* CPU must sample the external /INT line (level) driven only by IRQController.
* ROM configures IM 1; when interrupts enabled and /INT asserted, CPU vectors to `0x0038` (CPU core behavior).
* EI delay must be honored by the CPU core (do not emulate EI delay in IRQController).

You must ensure the CPU wrapper provides a way to set the interrupt line each scanline (or more often if your CPU core requires it):

```cpp
cpu_.SetIntLine(irq_.IntLineAsserted()); // level-sensitive
```

======================================================================
CALL ORDER / EXECUTION ORDER (SCANLINE-DRIVEN; DETERMINISTIC)

Per scanline (Scheduler-driven), implement exactly:

1. Scheduler computes `cycles_this_line` using the Phase 3 fractional accumulator (already implemented).

2. **Scanline start hook** (new, Phase 4): `OnScanlineStart(scanline)`:

    * If synthetic trigger conditions match, call `irq_.Raise(mask)`.
    * Then call `irq_.PreCpuUpdate()` to recompute /INT before CPU runs this line.
    * Then drive CPU’s /INT input: `cpu_.SetIntLine(irq_.IntLineAsserted())`.

3. CPU executes exactly `cycles_this_line` T-states:

    * `cpu_.StepTStates(cycles_this_line)` (or equivalent).
    * All I/O reads/writes during execution go through Bus; IRQ_ACK/ENABLE writes update IRQController.

4. After CPU step finishes:

    * Call `irq_.PostCpuUpdate()`.
    * Call `cpu_.SetIntLine(irq_.IntLineAsserted())` again (keeps external line consistent for next scanline; also ensures immediate drop is visible).

5. (Phase 4 strict scope) No other interrupt sources are raised here.

6. Advance scanline counter.

Rationale: scanline-start event asserts IRQ at a deterministic boundary as required by timing model for sources that fire at scanline start, while post-CPU update ensures ACK clears drop /INT immediately.

======================================================================
SYNTHETIC IRQ TRIGGER STRATEGY (SOFTWARE-DRIVEN ONLY)

Implement a deterministic, test-harness-only synthetic IRQ generator:

* Choose source bit: use **TIMER pending** (bit 1) per skeleton.
* Trigger rule: **once per frame**, at **start of scanline 10**, raise TIMER pending.
* Behavior:

    * Always latch pending regardless of enable (pending is a status latch).
    * /INT asserts only if TIMER bit is enabled in IRQ_ENABLE.

Pseudo-hook:

```cpp
// scheduler.cpp (or console scanline loop)
void Console::OnScanlineStart(uint16_t scanline) {
  if (scanline == 10 && !synthetic_fired_this_frame_) {
    irq_.Raise(static_cast<uint8_t>(IrqBit::Timer));
    synthetic_fire_count_++;
    synthetic_fired_this_frame_ = true;
  }

  if (scanline == 0) synthetic_fired_this_frame_ = false; // new frame boundary
  irq_.PreCpuUpdate();
  cpu_.SetIntLine(irq_.IntLineAsserted());
}
```

No other automatic sources:

* Do NOT raise VBlank pending (Phase 5).
* Do NOT implement scanline compare register 0x86 behavior (stubbed).

======================================================================
TEST ROM / ISR REQUIREMENTS (NO FULL ROM CODE; JUST CONTRACT)

The Phase 4 validation ROM must:

* Configure IM 1 and enable interrupts (EI).
* Enable TIMER interrupt source by writing IRQ_ENABLE with bit 1 set:

    * `OUT (0x81), 0b0000_0010`.
* Provide an ISR at address `0x0038` that:

    1. Optionally reads IRQ_STATUS (0x80) for debugging (must not clear).
    2. ACKs TIMER pending by writing write-1-to-clear to IRQ_ACK (0x82):

        * `OUT (0x82), 0b0000_0010`.
    3. Returns with `RETI`.
* Increment a counter in Work RAM in the ISR so emulator can verify ISR entries (e.g., a 16-bit count at 0xC000).

ROM must not rely on VBlank or any other source.

======================================================================
DEBUG VISIBILITY (REQUIRED)

Expose and display (debug UI and/or log):

* Current scanline (from Scheduler).
* IRQController state:

    * pending bits
    * enable mask
    * /INT line asserted
* Synthetic trigger fire count (per frame total)
* ISR entry count (as observed)

Implementation options for ISR entry count observation:
A) Emulator-side observation (preferred, no ROM instrumentation needed beyond correct vector):

* Add a CPU debug hook: detect when CPU PC becomes `0x0038` (first instruction of ISR) and increment `isr_entry_count`.
* This is deterministic and does not require modifying ROM beyond placing ISR there.

B) ROM-side counter:

* Emulator reads the RAM counter at a known address every frame for display.

You may do both; they should match.

Log checks (at minimum):

* On each synthetic fire (scanline 10): log pending/enable/int state.
* On each ISR entry: log entry # and confirm IRQ_STATUS shows TIMER bit set before ACK.

======================================================================
HARD ASSERTIONS (MUST FAIL FAST)

Implement these assertions exactly:

1. Masking works:

* If `pending != 0` but `(pending & enable) == 0`, then `/INT` must be deasserted.
* Assert after `PreCpuUpdate()` and after `PostCpuUpdate()`.

2. Immediate drop:

* When pending bits are cleared such that `(pending & enable) == 0`, `/INT` must drop immediately in the same scanline step (achieve via recompute in Ack/WriteEnable and/or PostCpuUpdate).
* Add an assertion inside `Ack()`:

    * After clearing bits, recompute and ensure `int_line_` matches `(pending_ & enable_) != 0`.

3. No hidden sources:

* Assert that only the synthetic hook can call `Raise()` during Phase 4 (guard with a build-time flag or runtime “Phase4SyntheticOnly” boolean; if any other subsystem attempts to raise, assert).

======================================================================
PHASE 4 PASS/FAIL CRITERIA (MAP TO BRING-UP CHECKLIST)

To declare Phase 4 PASS, all must be true for extended runtime (e.g., thousands of frames):

* IRQController maintains pending bits (latched) and does not auto-clear on read.
* IRQ_ENABLE masks correctly; pending set while masked does not assert /INT (assertion #1).
* /INT asserted only when (pending & enable) != 0; level-sensitive until ACK (no edge-only behavior).
* CPU (in IM 1) vectors to `0x0038` when /INT asserted and interrupts enabled.
* EI delay honored by CPU core (validated indirectly: no ISR until after EI + following instruction).
* IRQ_ACK clears pending bits (write-1-to-clear) and /INT drops immediately when no enabled pending remains.
* ISR entry count matches synthetic fire count exactly (no missing or extra interrupts).

If any assertion trips, Phase 4 FAILS; fix before proceeding.

======================================================================
DELIVERABLES SUMMARY (WHAT YOU MUST CHANGE/ADD)

* New `IRQController` module with interfaces above.
* Bus I/O decoding for ports 0x80/0x81/0x82 exactly; all else in 0x83–0x8F stubbed 0xFF/ignored.
* Scheduler/Console scanline loop: add `OnScanlineStart()` synthetic trigger + `PreCpuUpdate()` before CPU runs, and `PostCpuUpdate()` after CPU runs.
* CPU wrapper: ensure /INT line is sourced only from IRQController and updated at least at the two points described.
* Debug state struct + debug panel/log output fields (scanline, pending, enable, /INT, synthetic count, ISR count).
* Hard assertions described above.

Do not implement DMA, VBlank IRQ, scanline compare, sprite overflow IRQ, or audio/video rendering beyond existing Phase 0 test pattern path.
