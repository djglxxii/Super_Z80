# Super_Z80 Emulator Architecture (Authoritative)

**Status:** Locked
**Audience:** Emulator implementation and long-term maintenance
**Purpose:** Define clear module boundaries, ownership rules, and execution order so the emulator remains correct, debuggable, and maintainable.

If code behavior contradicts this document, **the code is wrong**.

---

## 1. Architectural Philosophy

1. **Motherboard model**

   * The emulator represents a *single console motherboard*.
   * CPU, video, audio, DMA, and cartridge are devices attached to it.

2. **Bus-centric design**

   * All CPU memory and I/O access flows through a single Bus abstraction.
   * Devices never talk to each other directly unless explicitly stated.

3. **Time flows in one direction**

   * Time advances via the **Scheduler** using the locked timing model.
   * No subsystem may “pull” time independently.

4. **Debug visibility over cleverness**

   * All state transitions must be observable and inspectable.

---

## 2. Top-Level Module Diagram (Conceptual)

```
+----------------------+
|   SuperZ80Console    |
|----------------------|
| - Scheduler          |
| - Bus                |
| - IRQController      |
| - Cartridge          |
| - PPU (Video)        |
| - APU (Audio)        |
| - DMAEngine          |
| - InputController    |
+----------+-----------+
           |
           v
       +--------+
       |  Z80   |
       |  CPU   |
       +--------+
```

**Key rule:**
The CPU does not “own” the system. The **Console** does.

---

## 3. Core Modules and Responsibilities

### 3.1 `SuperZ80Console`

**Role:** System coordinator.

Owns:

* All devices
* System reset
* Frame stepping
* Top-level state

Responsibilities:

* Initialize subsystems
* Execute one scanline at a time
* Present completed frames
* Provide debug hooks

Must NOT:

* Emulate instructions
* Decode registers
* Perform rendering or audio mixing

---

### 3.2 `Scheduler`

**Role:** Timekeeper and event sequencer.

Responsibilities:

* Advance time scanline-by-scanline
* Allocate CPU T-states using fractional accumulator
* Trigger:

  * VBlank start/end
  * Scanline compare
  * Frame boundaries
* Call device “tick” methods in correct order

Must:

* Follow `super_z80_emulation_timing_model.md` exactly

Must NOT:

* Access memory directly
* Know about register layouts

---

### 3.3 `Bus`

**Role:** Sole access path for CPU reads/writes.

Responsibilities:

* Decode:

  * Memory reads/writes
  * I/O port reads/writes
* Dispatch to:

  * Work RAM
  * Cartridge
  * VRAM window
  * I/O registers

Rules:

* CPU *never* touches devices directly
* Devices never bypass the bus for CPU-visible state

---

### 3.4 `Z80CPU`

**Role:** Instruction execution only.

Responsibilities:

* Execute Z80 instructions
* Consume T-states
* Respond to `/INT` line
* Expose registers for debug

Must:

* Be cycle-accurate
* Be driven externally (scheduler decides how many cycles)

Must NOT:

* Know about scanlines
* Know about video/audio
* Trigger DMA
* Acknowledge IRQs on its own

---

### 3.5 `IRQController`

**Role:** Central interrupt arbiter.

Responsibilities:

* Maintain IRQ pending bits
* Apply IRQ_ENABLE mask
* Assert/deassert `/INT` line to CPU
* Handle IRQ_ACK writes

Rules:

* IRQs are **level-sensitive**, not edge-triggered
* Pending flags persist until explicitly cleared

---

### 3.6 `PPU` (Video Subsystem)

**Role:** Arcade-style tile/sprite renderer.

Responsibilities:

* Maintain VRAM
* Maintain palette RAM
* Evaluate sprites per scanline
* Render visible scanlines into framebuffer
* Track VBlank state
* Raise sprite overflow IRQs

Must:

* Render strictly in scanline order
* Apply register changes at defined boundaries (scanline start)

Must NOT:

* Advance time
* Perform DMA
* Access CPU memory directly

---

### 3.7 `DMAEngine`

**Role:** Controlled RAM → VRAM transfer unit.

Responsibilities:

* Validate DMA legality (VBlank only)
* Perform instantaneous copy
* Raise DMA_DONE IRQ (if enabled)

Rules:

* CPU is never stalled
* DMA never partially executes
* DMA never affects visible scanlines

---

### 3.8 `APU` (Audio Subsystem)

**Role:** Audio chip coordination and mixing.

Owns:

* PSG core
* YM2151 core
* PCM channels
* Audio mixer

Responsibilities:

* Advance audio chips based on elapsed time
* Generate audio samples
* Feed host audio buffer

Must:

* Stay synchronized with Scheduler
* Avoid frame-based audio stepping

Must NOT:

* Depend on video frame boundaries

---

### 3.9 `Cartridge`

**Role:** Game media abstraction.

Responsibilities:

* Expose ROM data
* Expose optional SRAM
* Implement mapper logic
* Enforce reset mapping

Rules:

* All ROM access goes through Bus
* Cartridge owns its own header metadata

---

### 3.10 `InputController`

**Role:** Host input → console input translation.

Responsibilities:

* Poll host input (via SDL)
* Translate into controller bitfields
* Expose read-only registers to Bus

Must:

* Be stateless across frames unless hardware says otherwise

---

## 4. Execution Order (Per Scanline)

For each scanline:

1. Scheduler computes CPU cycles for this line
2. CPU executes for those T-states
3. IRQController updates `/INT` line
4. If visible scanline:

   * PPU renders scanline
5. If scanline == 192:

   * Enter VBlank
   * Raise VBlank IRQ
6. DMAEngine processes queued DMA (if any)
7. APU advances audio time
8. Advance to next scanline

**Order is mandatory.**

---

## 5. Reset Sequence (System Power-On)

1. Console resets all subsystems
2. Cartridge maps Bank 0 at `0x0000`
3. PPU clears internal latches
4. IRQController clears pending bits
5. CPU reset vector executes
6. No interrupts until software enables them

---

## 6. Debug & Introspection Requirements

Every major module must expose:

* A read-only debug state struct
* No side effects when inspected

Required debug views:

* CPU registers
* Current scanline
* VBlank state
* IRQ pending/masked bits
* VRAM dump
* Tile viewer
* Sprite list

---

## 7. Forbidden Couplings (Hard Rules)

The following are **explicitly disallowed**:

* CPU calling PPU methods
* PPU reading CPU RAM directly
* Audio stepping per frame
* DMA triggered implicitly by writes
* Scheduler skipping scanlines
* Devices advancing time autonomously

Violating these rules introduces non-determinism.

---

## 8. Why This Architecture Is Locked

* Matches real arcade board thinking
* Scales cleanly as features are added
* Makes timing bugs diagnosable
* Prevents “god object” emulator collapse

This architecture is the **structural equivalent** of the timing model.

---
