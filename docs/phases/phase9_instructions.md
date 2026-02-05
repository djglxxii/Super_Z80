## CODING-AGENT PROMPT — Super_Z80 Bring-Up Phase 9: Diagnostic Cartridge ROM (Full Pass)

### Goal

Implement and run the **Super_Z80 Diagnostic Cartridge ROM** end-to-end. The emulator is **incorrect** if the ROM does not pass. Do not add new features outside strict scope; only wire/patch missing glue required by the ROM spec.

**Strict scope (must obey):**

* NO Plane B
* NO sprites
* NO window/HUD split
* NO APU/audio output
* Use existing: CPU+Bus, Scheduler/VBlank, IRQController, DMAEngine, PPU Plane A, Palette
* Validation-first, debug visibility mandatory

**ROM hard constraints:**

* ROM size ≤ 32KB
* No mapper usage (bank 0 only)
* Entry point 0x0000
* IM 1 interrupt vector @ 0x0038

---

## 1) Diagnostic ROM artifact (assembly) + build output

### 1.1 File layout (ROM source + build)

Create a new folder:

```
/roms/diag_cart/
  README.md
  src/
    diag_cart.asm
    hw.inc            ; port constants + RAM addresses + page sizes
    tiles.inc         ; tile pattern bytes (4 tiles, 4bpp)
    palette.inc       ; palette bytes (buffer matches DMA length)
    tilemap.inc       ; 32x24 tilemap data or generator macros
  build/
    Makefile          ; or CMake custom target calling assembler
    out/
      diag_cart.bin
```

### 1.2 Assembler choice + build instructions

Use a standard Z80 assembler with binary output (pick one and document it in README):

* `sjasmplus` (recommended for easy `OUTPUT` to `.bin`), or
* `z80asm`

Build must produce a raw **flat** `diag_cart.bin` that is mapped as the emulator’s cartridge ROM (no banking).

---

## 2) ROM exact behavior (must match spec)

### 2.1 Memory usage plan (RAM addresses are fixed)

Use Work RAM range `0xC000–0xFFFF`.

Mandatory RAM variables (exact addresses):

* `FRAME_COUNTER` (16-bit) @ `0xC000`
* `VBLANK_COUNT` (16-bit or 8-bit; but stored starting) @ `0xC002`
* `ISR_ENTRY_COUNT` @ `0xC004`
* `SCRATCH` @ `0xC006`
* DMA source buffer @ `0xC100–0xC1FF`
* Tilemap buffer @ `0xC200–0xC2FF`

### 2.2 CPU initialization (Reset → main)

On reset, ROM must do:

* `DI`
* `SP = 0xFFFE`
* Clear RAM region `0xC000–0xC3FF`
* `IM 1`
* `EI` only after hardware setup is complete

No `HALT` in main loop (explicitly forbidden).

### 2.3 Video initialization (Plane A only)

Write video control registers so:

* Display enable = ON
* Plane A enabled
* Plane B disabled
* Sprites disabled

Use these ports (authoritative port map):

* `VDP_STATUS` = port `0x10` (VBlank flag is 1 during scanlines 192–261)
* `VDP_CTRL`   = port `0x11`

Base selectors (page indices; define page size in `hw.inc`):

* `PLANE_A_BASE` = port `0x16`
* `PATTERN_BASE` = port `0x18`

> ROM requirement: pick deterministic base page values (e.g., 0 for both) and ensure emulator interprets them consistently as page indices.

### 2.4 Palette initialization (DMA required; VBlank-only)

Prepare palette data bytes in RAM `0xC100–0xC11F` as specified:

* Entry 0: black
* Entry 1: red
* Entry 2: green
* Entry 3: blue
* Entry 4: white
* Remaining: gradient or repeated pattern

Then **during VBlank only** do **DMA RAM → Palette RAM**, length = palette byte count.

DMA registers (ports):

* `DMA_SRC_LO/HI` = `0x30/0x31`
* `DMA_DST_LO/HI` = `0x32/0x33`
* `DMA_LEN_LO/HI` = `0x34/0x35`
* `DMA_CTRL` = `0x36` (START bit 0; queue policy bit 2)

**Important:** The ROM spec requires DMA to **Palette RAM** (not just VRAM). If current DMAEngine only supports RAM→VRAM, add the minimum extension:

* Define a DMA destination address convention that can target **Palette RAM** as well as VRAM.
* Keep it simple and deterministic (example convention you may implement):

  * If `DMA_DST_HI` selects an address range reserved for palette (e.g., 0xC0xx), treat destination as palette RAM index.
  * Otherwise treat destination as VRAM address.
* Document the convention in `/roms/diag_cart/src/hw.inc` and in emulator code comments.
* Enforce “DMA applies only during VBlank” and “outside VBlank: queue or ignore depending on policy bit”.

### 2.5 Tile patterns (exactly 4 tiles; DMA into pattern base during VBlank)

Tile data requirements:

* Exactly 4 tiles, 8×8, 4bpp
* Placed in RAM at `0xC120–0xC17F`
* Patterns:

  * Tile 0: solid color (palette 1)
  * Tile 1: checkerboard
  * Tile 2: vertical stripes
  * Tile 3: horizontal stripes

During VBlank, DMA these bytes into VRAM at the **pattern base** selected by `PATTERN_BASE`.

### 2.6 Tilemap (Plane A) 32×24; DMA into Plane A tilemap base during VBlank

Tilemap requirements:

* Build a 32×24 map in RAM (use the tilemap buffer region indicated by spec)
* Each row repeats: `[0 1 2 3 0 1 2 3 ...]`
* Attributes:

  * Palette 0
  * No flip
  * Normal priority
* DMA tilemap into VRAM during VBlank to the base selected by `PLANE_A_BASE`.

### 2.7 Interrupt system validation (IM 1 ISR @ 0x0038)

Enable VBlank IRQ and then `EI`.

System IRQ ports:

* `IRQ_STATUS` = `0x80` (bit 0 = VBlank pending)
* `IRQ_ENABLE` = `0x81`
* `IRQ_ACK` = `0x82` (write-1-to-clear)

ISR at `0x0038` must:

1. Increment `ISR_ENTRY_COUNT`
2. Increment `VBLANK_COUNT`
3. Toggle palette entry 1 between **Red ↔ Blue**

   * Use Palette ports (`PAL_ADDR=0x1E`, `PAL_DATA=0x1F`) for the toggle write, so the effect is visible and tests mid-frame palette semantics. (Palette ports are defined in the skeleton.)
4. ACK VBlank via `IRQ_ACK` (write bit0 = 1)
5. `RETI`

Main loop runs forever (busy loop); optionally updates `FRAME_COUNTER` once per frame by polling VBlank state (e.g., `VDP_STATUS` VBlank flag) or by deriving from ISR events.

---

## 3) Emulator integration (cartridge loading + reset mapping)

### 3.1 Load diagnostic ROM as cartridge ROM image (no banking)

Add a cartridge selection path for `diag_cart.bin`:

* Place output in a known path (e.g., `roms/diag_cart/build/out/diag_cart.bin`)
* Emulator loads this as the cartridge ROM backing store.

### 3.2 Reset rule: bank 0 mapped at 0x0000

On reset, enforce mapper defaults and “bank 0 at reset vector 0x0000”.

Even if the emulator has bank switching support, this diagnostic must run with **no mapper usage** and must start at 0x0000 with fixed contents.

---

## 4) Validation instrumentation (debug-first; must exist)

### 4.1 On-screen observable

Render Plane A only:

* Stable 256×192 image
* Correct repeating tile pattern across 32×24 tiles
* Entire-screen **palette pulse exactly once per frame** (entry 1 toggles red ↔ blue)

### 4.2 Mandatory debug counters/telemetry in emulator UI/log

Add a debug overlay or structured log with:

* `frame_counter` (emulator-owned): increments once per frame at **VBlank start**
* `vblank_isr_count` (ROM-owned): read RAM at `0xC002` or `0xC004` depending on which you label; show both counters explicitly by address
* `irq_pending_bits`: `IRQ_STATUS` readback (bit0 should assert during VBlank IRQ pending until ACK)
* `last_vblank_scanline`: must always be **192** (VBlank begins at scanline 192)

Also display VBlank state from `VDP_STATUS` where applicable (VBlank true during scanlines 192–261).

### 4.3 Optional “self-check” mode (recommended; make it togglable)

Implement an emulator debug option (e.g., `--diag-selfcheck`) that periodically:

* Reads RAM counters at `0xC000`, `0xC002`, `0xC004`
* Asserts invariants:

  * ISR count increments **exactly once per frame** over long runtime (allow an initial offset during warm-up; once stable, delta(frame) == delta(isr))
  * No double IRQs (ISR count should not increase by >1 between consecutive frames)
  * No missed frames (ISR count should not lag frame_counter beyond a small allowed startup window)

If a check fails:

* Break into debugger or print a hard error with:

  * current scanline
  * IRQ_STATUS/IRQ_ENABLE
  * PC at failure
  * last IRQ_ACK write observed

---

## 5) Minimum emulator glue required (do not add features beyond this)

### 5.1 Enforce VBlank timing + determinism

You must satisfy:

* VBlank begins at scanline **192** and ends at **261**.
* `VDP_STATUS` VBlank bit reflects that window.
* VBlank IRQ should fire **once per frame**, latched until ACK via `IRQ_ACK` write-1-to-clear.

### 5.2 DMA legality rules must be exact

DMA_CTRL behavior must comply:

* If START written during VBlank: execute immediately.
* If START written outside VBlank:

  * If QUEUE_IF_NOT_VBLANK=1: queue for next VBlank.
  * Else: ignore (optionally log).
* DMA must not cause mid-frame visible corruption; the diagnostic ROM uses this to detect incorrect behavior.

### 5.3 Palette mid-frame update semantics

Palette writes must not tear/glitch:

* Apply palette writes at a consistent boundary (recommended by skeleton: next scanline boundary).
* Ensure the once-per-frame toggle is stable and exactly once per frame visually.

---

## 6) Phase 9 acceptance checklist (what to observe; hard pass/fail)

### Visual (must pass)

* Correct 32×24 tilemap pattern is visible and stable
* No flicker
* Palette pulse (red ↔ blue) occurs exactly once per frame

### Timing + counters (must pass)

* ISR fires exactly once per frame over minutes (no drift)
* `ISR_ENTRY_COUNT` and `VBLANK_COUNT` increase monotonically and match emulator `frame_counter` over time (allow small initial offset only)
* `last_vblank_scanline` always reports 192
* No double IRQs (no frame where ISR count jumps by 2)
* No missed frames (no long-term divergence)

### Stability (must pass)

* No visual corruption after extended runtime (minutes)
* No timing drift after minutes of runtime

---

## 7) Notes to implementer (do not skip)

* Do not “fix” failures by loosening rules. The diagnostic ROM is the authority: if it fails, the emulator is wrong.
* Keep Plane B/sprites/window disabled in both ROM configuration and emulator runtime.
* Any required DMA-to-palette convention must be documented and used consistently by ROM + emulator.

---
