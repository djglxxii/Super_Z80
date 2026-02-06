# Super_Z80 Diagnostic Cartridge ROM Specification

## Canonical Validation ROM (v1.3 – Emulator-Aligned)

**Status:** Mandatory validation ROM
**Audience:** Emulator correctness verification
**Purpose:** Prove that the Super_Z80 emulator implements the documented hardware contract correctly and deterministically.

This ROM is **not a game**.
This ROM is **not a stress test**.
This ROM is a **hardware exerciser**.

If the emulator fails this ROM, **the emulator is wrong**.

---

## 1. Design Intent (Clarified)

The diagnostic ROM validates **only documented, supported hardware behavior**.

It must **not**:

* Probe undefined behavior
* Rely on CPU-visible VRAM access
* Use mid-scanline effects
* Depend on uninitialized state
* Exploit emulator conveniences

It must validate:

1. Z80 instruction correctness
2. Interrupt Mode 1 delivery and acknowledgment
3. Deterministic scanline and VBlank timing
4. DMA legality and queuing rules
5. VRAM population via DMA only
6. Plane A tile rendering
7. Palette RAM format and timing
8. IRQ latch / mask / ACK behavior
9. Long-run frame pacing stability

**Audio is explicitly excluded.**

---

## 2. ROM Constraints (Locked)

| Parameter        | Value              |
| ---------------- | ------------------ |
| ROM size         | ≤ 32 KB            |
| Mapper usage     | None (Bank 0 only) |
| SRAM             | Not used           |
| Entry point      | `0x0000`           |
| Interrupt vector | IM 1 @ `0x0038`    |

The ROM must run entirely from fixed cartridge ROM.

---

## 3. Memory Usage Plan (CPU-Visible Only)

### Work RAM (`0xC000–0xFFFF`)

| Address         | Purpose                     |
| --------------- | --------------------------- |
| `0xC000–0xC001` | Frame counter (16-bit)      |
| `0xC002–0xC003` | VBlank counter (16-bit)     |
| `0xC004–0xC005` | ISR entry counter (16-bit)  |
| `0xC006–0xC00F` | Scratch / temporaries       |
| `0xC100–0xC17F` | DMA source: palette + tiles |
| `0xC180–0xC2FF` | DMA source: Plane A tilemap |

> **Important:**
> The ROM must never assume CPU-readable or writable VRAM.
> All video memory interaction occurs via **DMA only**.

---

## 4. Startup Sequence (Reset → Main)

### 4.1 CPU Initialization

On reset, the ROM must:

1. Disable interrupts (`DI`)
2. Initialize stack pointer (`SP = 0xFFFE`)
3. Clear RAM region `0xC000–0xC2FF`
4. Set interrupt mode:

   * `IM 1`
5. Leave interrupts disabled until **all video state is initialized**

---

## 5. Video Initialization (Plane A Only)

### 5.1 Global Video Control

Write video control registers to:

* Enable display
* Enable Plane A
* Disable Plane B
* Disable sprites

Expected state:

* Display enabled
* Background initially black

No assumptions may be made about VRAM or palette contents at reset.

---

## 6. Palette Initialization (DMA-Only, VBlank-Safe)

### 6.1 Palette Data Preparation

Prepare palette data in RAM (`0xC100+`), formatted exactly as:

* 128 entries
* 9-bit RGB
* Byte-addressed
* Two bytes per entry

Minimum required entries:

| Entry | Color |
| ----- | ----- |
| 0     | Black |
| 1     | Red   |
| 2     | Green |
| 3     | Blue  |
| 4     | White |

Remaining entries may be any deterministic pattern.

---

### 6.2 Palette DMA Rules (Validated)

During **VBlank only**:

* Program DMA:

  * Source: RAM
  * Destination: Palette RAM
  * Length: palette byte count
  * `DST_IS_PALETTE = 1`

**Required emulator behavior:**

* Palette DMA outside VBlank must **not** execute
* Palette updates must become visible only at **scanline boundaries**
* No mid-scanline color changes are permitted

---

## 7. Tile Pattern Setup (Plane A)

### 7.1 Tile Definitions

Exactly **4 tiles**, 8×8, 4bpp:

| Tile | Description                   |
| ---- | ----------------------------- |
| 0    | Solid color (palette entry 1) |
| 1    | Checkerboard                  |
| 2    | Vertical stripes              |
| 3    | Horizontal stripes            |

Tile pattern bytes stored in RAM at `0xC120–0xC17F`.

---

### 7.2 Tile Pattern DMA

During VBlank:

* DMA RAM → VRAM (pattern base)
* Length = exact tile byte count

Expected:

* Correct tile decoding
* No corruption
* No CPU access to VRAM

---

## 8. Tilemap Setup (Plane A)

### 8.1 Tilemap Layout

Prepare a **32×24 tilemap** in RAM:

```
Row 0–23:
[ 0 1 2 3 0 1 2 3 ... ]
```

Tile attributes:

* Palette 0
* No flip
* Default priority

DMA tilemap into VRAM during VBlank.

---

## 9. Interrupt System Validation

### 9.1 VBlank IRQ Enable

After all video and palette setup:

1. Enable VBlank interrupt in IRQ_ENABLE
2. Execute `EI`

---

### 9.2 IM 1 ISR (`0x0038`)

The interrupt service routine must:

1. Increment ISR entry counter
2. Increment VBlank counter
3. Toggle palette entry **1**:

   * Red ↔ Blue
4. Write to `IRQ_ACK` to clear VBlank pending
5. Execute `RETI`

**Expected result:**

* Entire screen pulses color once per frame
* No flicker
* No tearing

---

## 10. Main Loop Behavior

The main loop must:

* Run forever
* Avoid `HALT` (to expose timing errors)
* Increment frame counter exactly once per frame
  (via VBlank polling or ISR correlation)

---

## 11. Expected Emulator Observables

### 11.1 Visual

* Stable 256×192 image
* Correct tile patterns
* Palette toggles exactly once per frame

### 11.2 Timing

* Exactly one ISR per frame
* VBlank asserted at scanline 192
* No missed or double IRQs
* Stable pacing over minutes of runtime

### 11.3 Debug Expectations

With debug tools enabled:

* Scanline counter reaches 192 → VBlank asserted
* DMA executes only during VBlank
* IRQ pending clears only after ACK
* CPU PC enters `0x0038` exactly once per frame

---

## 12. Failure Modes (Interpreted)

| Symptom                 | Meaning                         |
| ----------------------- | ------------------------------- |
| Black screen            | DMA failed or display disabled  |
| Mid-frame changes       | Palette timing violation        |
| Flickering              | IRQ timing or ACK failure       |
| Multiple ISRs per frame | IRQ latch or mask error         |
| No ISR                  | IM 1 not wired correctly        |
| Scrambled tiles         | Tile decode or DMA source error |

---

## 13. Hard Pass Criteria

The emulator **passes** when all are true:

1. Correct tile pattern visible
2. Palette pulses once per frame
3. ISR count == frame count over time
4. No visual corruption
5. No timing drift after extended runtime

Only after this ROM passes is the emulator considered:

> **Structurally correct and demo-ready**

---

## 14. Why This ROM Exists

This ROM:

* Locks the timing model
* Proves DMA legality
* Proves interrupt correctness
* Prevents silent regressions

It is the **ground truth** for emulator correctness.

---

