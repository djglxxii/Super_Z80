# Super_Z80 Emulator Timing Model (Authoritative)

**Status:** Locked (unless explicitly revised)
**Audience:** Emulator implementation (CPU, video, audio, DMA, IRQ)
**Purpose:** Define *exactly when* things happen so behavior is deterministic and debuggable.

If emulator behavior contradicts this document, **the code is wrong**, not the document.

---

## 1. Guiding Principles

1. **CPU is not the master clock**
   The emulator is driven by a **video-beam timeline** (scanlines), with CPU and audio advancing relative to it.

2. **Cycle accuracy where it matters**

   * CPU: T-state accurate
   * Video: scanline accurate (not pixel-level)
   * Audio: sample-accurate over time, chip-accurate internally

3. **Determinism over convenience**
   No “run CPU then draw frame” shortcuts. All subsystems advance in lockstep against the same timebase.

4. **Debug-first design**
   All timing decisions must be inspectable (current scanline, cycle counters, IRQ pending flags).

---

## 2. Clock Sources

### Master Clock

* **Crystal:** 21.47727 MHz (NTSC colorburst)

This is the only true source of time. Everything else derives from it.

---

### Derived Clocks

| Subsystem           | Frequency                            | Notes                            |
| ------------------- | ------------------------------------ | -------------------------------- |
| CPU (Z80H)          | 21.47727 MHz ÷ 4 = **5.3693175 MHz** | Matches hardware intent          |
| YM2151 (OPM)        | 21.47727 MHz ÷ 6 = **3.579545 MHz**  | Canonical Yamaha NTSC clock      |
| PSG (SN76489-style) | **3.579545 MHz**                     | Consistent with SMS/arcade usage |
| Video line rate     | **15,734.264 Hz**                    | NTSC standard                    |
| Frame rate          | ~**60.098 Hz**                       | Derived from line rate           |

**Important:**
We do **not** force exactly 60.000 Hz. The frame rate is derived from scanlines to prevent long-term drift.

---

## 3. Video Beam Model

### Scanline Geometry

| Parameter                 | Value                  |
| ------------------------- | ---------------------- |
| Total scanlines per frame | **262**                |
| Visible scanlines         | **192** (lines 0–191)  |
| VBlank scanlines          | **70** (lines 192–261) |
| Active resolution         | 256 × 192              |

### Scanline Phases (conceptual)

Each scanline is treated as a single atomic unit for scheduling purposes:

* CPU runs for that line’s allocated cycles
* Video logic processes that line (if visible)
* IRQs may assert at defined boundaries

We **do not** simulate horizontal blank or pixel clocks.

---

## 4. CPU Scheduling Model

### CPU Time Allocation

CPU execution is distributed **per scanline**, not per frame.

Compute:

```
cpu_cycles_per_line = CPU_HZ / LINE_HZ
```

This is fractional.

### Fractional Accumulator Rule (Mandatory)

* Maintain a floating-point (or fixed-point) accumulator
* Each scanline:

  1. accumulator += cpu_cycles_per_line
  2. cycles_this_line = floor(accumulator)
  3. accumulator -= cycles_this_line
  4. Run CPU for exactly `cycles_this_line` T-states

This guarantees:

* Correct average frequency
* Deterministic long-run timing
* No jitter accumulation

---

## 5. Interrupt Timing (IM 1 Only)

### Interrupt Sources

All sources are **wired-OR** to Z80 `/INT`:

* VBlank
* Programmable timer
* Optional scanline compare
* Optional sprite overflow

### VBlank Interrupt (Primary)

* **Asserts at start of scanline 192**
* Triggers Z80 IM 1 sequence
* Z80 vectors to `0x0038`
* Interrupt acknowledge costs **13 T-states** (handled by CPU core)

### Scanline Compare Interrupt (Optional)

* Triggered at **start of configured scanline**
* Intended for split-screen / HUD effects

### Acknowledgement Rules

* ISR must clear the relevant status bit via I/O register
* If not cleared, IRQ remains asserted
* Emulator must not auto-clear IRQs

### EI Delay Rule

* EI enables interrupts **after the following instruction**
* Must be respected exactly (delegated to CPU core)

---

## 6. DMA Timing Rules (Critical)

### DMA Type

* **RAM → VRAM only**
* Initiated via I/O register write

### DMA Legality

* **Allowed only during VBlank** (scanlines 192–261)
* Illegal outside VBlank

### Emulator Behavior

* If DMA is triggered **during VBlank**:

  * Execute instantly
  * Changes become visible next frame
* If DMA is triggered **outside VBlank**:

  * Either:

    * Queue until VBlank (recommended), or
    * Ignore and set an error/status flag
* CPU is **not stalled** by DMA

**Never allow partial or mid-frame VRAM changes to affect rendering.**

---

## 7. Video Rendering Schedule

### Rendering Model

* **Scanline-based renderer**
* For each visible scanline:

  1. Render Plane A background
  2. Render Plane B background
  3. Evaluate sprites for that scanline (limit = 16)
  4. Apply priority and palette
  5. Write final pixels to framebuffer

### Sprite Evaluation

* Sprite list is scanned once per line
* First 16 qualifying sprites are drawn
* Overflow may raise optional IRQ

---

## 8. Audio Synchronization Model

### Audio Is Time-Driven, Not Frame-Driven

* Audio chips advance based on **real elapsed cycles**
* Output is buffered for the host audio device

### Practical Rule

* Accumulate CPU cycles
* Convert elapsed time into audio samples
* Generate samples at:

  * 44,100 Hz (recommended)
* Small resampling/stretching is acceptable to avoid crackle

Audio correctness > exact sample alignment to frames.

---

## 9. Frame Boundary Rules

### Frame Start (Scanline 0)

* Clear VBlank flag
* Disallow DMA
* Prepare new frame buffer

### Frame End (Scanline 261 → 0)

* Present completed frame to host
* Do not reset cycle counters (they are continuous)

---

## 10. Invariants (Must Never Be Broken)

1. VBlank always begins at scanline 192
2. DMA never affects visible scanlines
3. CPU never “runs ahead” of the video beam
4. IRQ timing is edge-accurate at scanline boundaries
5. Fractional cycle loss never accumulates

If any of these are violated, emulator behavior is undefined.

---

## 11. Why This Document Exists

* This console has **no real hardware**
* Timing is therefore the *de facto hardware*
* Consistency matters more than guesswork
* This document is the single source of truth

---
