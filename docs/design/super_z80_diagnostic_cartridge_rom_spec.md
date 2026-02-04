# Super_Z80 Diagnostic Cartridge ROM Specification (Authoritative)

**Status:** Required for initial bring-up
**Audience:** Emulator validation, timing verification, debug tooling
**Purpose:** Provide a deterministic, minimal software workload that proves the emulator’s core systems function correctly.

This ROM is not a game. It is a **hardware exerciser**.

If the emulator fails this ROM, the emulator is wrong.

---

## 1. Design Goals

The diagnostic ROM must validate:

1. CPU execution correctness
2. Z80 IM 1 interrupt handling
3. Scanline/VBlank timing
4. DMA legality and effect
5. VRAM access correctness
6. Tile rendering (Plane A)
7. Palette loading and mid-frame updates
8. IRQ acknowledge behavior
9. Stable frame pacing

Audio is intentionally **not** required in phase 1.

---

## 2. ROM Constraints

| Parameter        | Value              |
| ---------------- | ------------------ |
| ROM size         | ≤ 32 KB            |
| Mapper usage     | None (Bank 0 only) |
| SRAM             | Not used           |
| Entry point      | `0x0000`           |
| Interrupt vector | IM 1 @ `0x0038`    |

The ROM must run entirely from fixed ROM without bank switching.

---

## 3. Memory Usage Plan

### Work RAM (`0xC000–0xFFFF`)

| Address         | Purpose                |
| --------------- | ---------------------- |
| `0xC000`        | Frame counter (16-bit) |
| `0xC002`        | VBlank counter         |
| `0xC004`        | ISR entry counter      |
| `0xC006`        | Scratch                |
| `0xC100–0xC1FF` | DMA source buffer      |
| `0xC200–0xC2FF` | Tilemap buffer         |

---

## 4. Startup Sequence (RESET → Main)

### 4.1 CPU Initialization

On reset:

* Disable interrupts (`DI`)
* Initialize stack pointer near top of RAM (`SP = 0xFFFE`)
* Clear RAM region `0xC000–0xC3FF`
* Set interrupt mode:

  * `IM 1`
  * `EI` **only after** hardware setup

---

## 5. Video Initialization (Plane A Only)

### 5.1 VDP Global Setup

Write registers to:

* Enable display
* Enable Plane A
* Disable Plane B
* Disable sprites

Expected outcome:

* Screen initially blank (black)

---

### 5.2 Palette Initialization (DMA Required)

Prepare palette data in RAM (`0xC100–0xC11F`):

* Entry 0: Black
* Entry 1: Red
* Entry 2: Green
* Entry 3: Blue
* Entry 4: White
* Remaining entries: gradient or repeated pattern

During **VBlank only**:

* DMA RAM → Palette RAM
* Length = number of palette bytes

**Failure indicators:**

* Palette writes outside VBlank must not take effect mid-frame
* Emulator must allow DMA only during VBlank

---

## 6. Tile Pattern Setup

### 6.1 Tile Pattern Data

Define **exactly 4 tiles**, 8×8, 4bpp:

| Tile | Pattern                 |
| ---- | ----------------------- |
| 0    | Solid color (palette 1) |
| 1    | Checkerboard            |
| 2    | Vertical stripes        |
| 3    | Horizontal stripes      |

Pattern bytes placed in RAM at `0xC120–0xC17F`.

---

### 6.2 Tile Pattern DMA

During VBlank:

* DMA tile patterns into VRAM pattern base

Expected:

* Tiles decode correctly in viewer/debugger
* No corruption

---

## 7. Tilemap Setup (Plane A)

### 7.1 Tilemap Layout

Populate a 32×24 tilemap buffer in RAM:

```
Row 0–23:
[ 0 1 2 3 0 1 2 3 ... ]
```

Each tile uses:

* Palette 0
* No flip
* Normal priority

DMA tilemap into VRAM during VBlank.

---

## 8. Interrupt System Validation

### 8.1 VBlank IRQ Enable

* Enable VBlank interrupt
* Enable global IRQs (`EI`)

---

### 8.2 IM 1 ISR (`0x0038`)

ISR must:

1. Increment `ISR_ENTRY_COUNT`
2. Increment `VBLANK_COUNT`
3. Toggle palette entry 1 between:

   * Red ↔ Blue
4. Write to `IRQ_ACK` to clear VBlank pending
5. `RETI`

**Expected visual result:**

* Entire screen color pulses at frame rate
* No tearing
* No missed frames

---

## 9. Main Loop Behavior

Main loop:

* Busy-loop forever
* Increment `FRAME_COUNTER` once per frame (poll VBlank flag or rely on ISR)

No `HALT` instruction (to avoid masking timing bugs).

---

## 10. Expected Emulator Observables

### 10.1 Visual

* Stable 256×192 image
* No flicker
* Tiles render correctly
* Palette toggles once per frame

### 10.2 Timing

* ISR fires exactly once per frame
* Frame pacing stable (no drift)
* No double IRQs
* No missed IRQs

### 10.3 Debugger Expectations

With debug UI enabled:

* Scanline counter reaches 192 → VBlank asserted
* DMA occurs only during VBlank
* IRQ pending flag clears after ACK
* CPU PC enters `0x0038` once per frame

---

## 11. Failure Modes and What They Mean

| Symptom                           | Likely Cause                                    |
| --------------------------------- | ----------------------------------------------- |
| Black screen                      | VRAM writes failing or display disabled         |
| Screen updates mid-frame          | DMA illegally allowed outside VBlank            |
| Flickering colors                 | IRQ timing wrong or palette applied immediately |
| ISR runs multiple times per frame | IRQ_ACK not working                             |
| No ISR                            | IM 1 not wired correctly                        |
| Tiles scrambled                   | Pattern decode or DMA source incorrect          |

---

## 12. Success Criteria (Hard Pass)

The emulator **passes** the diagnostic ROM when:

1. Screen displays correct tile pattern
2. Palette pulses once per frame
3. ISR count == frame count over time
4. No visual corruption
5. No timing drift after minutes of runtime

Only after this passes do we proceed to:

* Plane B
* Sprites
* Scroll
* Audio

---

## 13. Why This ROM Matters

This ROM:

* Proves the timing model
* Proves DMA rules
* Proves interrupt correctness
* Prevents false confidence

Every future bug is easier once this ROM is solid.

---

