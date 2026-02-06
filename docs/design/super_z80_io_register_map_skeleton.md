# Super_Z80 I/O Register Map Skeleton (Authoritative)

**Status:** Locked skeleton (bitfields may evolve, addresses should not)
**Audience:** Emulator implementation (Bus, PPU, APU, Mapper, IRQ controller, DMA)
**Purpose:** Provide a stable, code-ready register namespace so software and emulator agree on behavior.

---

## 0. Conventions

### I/O Space

* Z80 uses 8-bit I/O ports (`IN A,(n)` / `OUT (n),A`) and 16-bit decoded addressing may mirror by upper byte.
* For Super_Z80, we define registers in **0x00–0x9F** as canonical.
* Unmapped ports:

  * Reads return `0xFF`
  * Writes are ignored

### Read/Write Width

* All ports are **8-bit** unless explicitly marked as “pair” (hi/lo).

### Side-Effects and Timing Rules (must comply with timing model)

* **VBlank begins at scanline 192** and ends at 261.
* **DMA is only legal during VBlank**.
* IRQ sources are wired-OR into Z80 `/INT`.
* IRQs remain asserted until software clears them via ACK registers.

---

## 1. Port Decode Summary (High Level)

| Port Range  | Subsystem                                       |
| ----------- | ----------------------------------------------- |
| `0x00–0x0F` | Cartridge mapper / banking / cart SRAM control  |
| `0x10–0x1F` | Video control (planes, scroll, mode, status)    |
| `0x20–0x2F` | Sprite system (SAT base, sprite control/status) |
| `0x30–0x3F` | DMA control (RAM→VRAM, VBlank-only)             |
| `0x40–0x4F` | Controller input                                |
| `0x50–0x5F` | Expansion / reserved (light gun, future)        |
| `0x60–0x6F` | PSG (SN76489-style)                             |
| `0x70–0x7F` | YM2151 + PCM                                    |
| `0x80–0x8F` | Timer / system IRQ control                      |
| `0x90–0x9F` | Debug / reserved (optional)                     |

---

## 2. Cartridge / Mapper (`0x00–0x0F`)

### `0x00` — MAP_CTRL (R/W)

**Mapper control register**

* Bits (skeleton):

  * Enable/disable cart SRAM mapping
  * ROM bank size mode (if supported)
* Side effects:

  * Changes how CPU address ranges map to cartridge

### `0x01` — ROM_BANK_0 (R/W)

Select switchable ROM bank for CPU region `0x4000–0x7FFF` (or equivalent banked area per spec).

* Write: bank number
* Read: current bank number

### `0x02` — ROM_BANK_1 (R/W) *(optional, reserved)*

If future mapper supports a second banked window (leave as reserved now).

### `0x03` — SRAM_BANK (R/W) *(optional)*

If SRAM banking exists in carts.

### Reset Rule (Mandatory)

* On reset, emulator must enforce:

  * **Bank 0 mapped at reset vector** (`0x0000`)
  * Mapper state returns to defaults

---

## 3. Video Control (`0x10–0x1F`)

### `0x10` — VDP_STATUS (R)

Read-only status flags.

* Bits (skeleton):

  * `VBLANK` (1 during scanlines 192–261)
  * `SPR_OVERFLOW` (latched if more than 16 sprites on a scanline)
  * `SCANLINE_IRQ` pending flag
* Read side-effect (policy):

  * Reading does **not** clear flags (clearing is via IRQ_ACK)

### `0x11` — VDP_CTRL (R/W)

Global video enable/mode flags.

* Bits (skeleton):

  * Display enable
  * Plane enable masks
  * Sprite enable
  * Possibly priority mode
* Writes take effect:

  * Either immediately or at next scanline boundary (choose one and keep consistent; recommended: next scanline boundary)

### Plane A Scroll (fine)

* `0x12` — PLANE_A_SCROLL_X (R/W)
* `0x13` — PLANE_A_SCROLL_Y (R/W)

### Plane B Scroll (fine)

* `0x14` — PLANE_B_SCROLL_X (R/W)
* `0x15` — PLANE_B_SCROLL_Y (R/W)

### Tilemap / Pattern Base Select (VRAM base selectors)

* `0x16` — PLANE_A_BASE (R/W)
* `0x17` — PLANE_B_BASE (R/W)
* `0x18` — PATTERN_BASE (R/W)
  These select VRAM base addresses in implementation-defined units (e.g., 1KB pages). Keep as page indices.

### `0x19` — WINDOW_CTRL (R/W) *(optional)*

Used for HUD splits / windowing.

* Skeleton behavior:

  * Defines a scanline threshold and/or window region for fixed HUD

### VRAM Access Ports (optional but recommended for simplicity)

If CPU needs indirect VRAM writes (common in VDP designs), define:

* `0x1A` — VRAM_ADDR_LO (R/W)
* `0x1B` — VRAM_ADDR_HI (R/W)
* `0x1C` — VRAM_DATA (R/W)
* `0x1D` — VRAM_DATA_INC (W) *(optional; auto-increment write)*

**Rule:** VRAM writes outside VBlank are allowed only if spec says so. For now:

* Outside VBlank:

  * Writes are accepted but may cause undefined behavior in real hardware.
  * Emulator policy: allow writes but they only affect *next scanline* at earliest (safe), OR optionally warn in debug.
* DMA is still restricted to VBlank.

### Palette Access Ports

* `0x1E` — PAL_ADDR (R/W)
* `0x1F` — PAL_DATA (R/W)

PAL_ADDR (0x1E)
- Byte-addressed
- Valid range: 0x00–0xFF
- Each palette entry occupies two bytes:
  - Even address: high bits
  - Odd address: low bits
  
Palette format:

* 9-bit RGB (3-3-3)
* Stored packed across bytes (exact packing TBD)
* Emulator must support mid-frame palette updates (applies starting next pixel/scanline depending on scheduling; recommended: next scanline boundary)


---

## 4. Sprite System (`0x20–0x2F`)

### `0x20` — SPR_CTRL (R/W)

* Sprite enable
* Sprite size mode selection (8×8, 8×16, 16×16)
* Priority behavior mode (if any)

### Sprite Attribute Table (SAT) base

* `0x21` — SAT_BASE (R/W)
  Select VRAM base page/index where sprite attributes are read.

### `0x22` — SPR_STATUS (R)

* `OVERFLOW` latched
* possibly `COLLISION` if ever added (leave reserved)

### Reserved

* `0x23–0x2F` reserved for future extensions (sprite zoom, affine, etc. — not planned)

---

## 5. DMA Control (`0x30–0x3F`)

DMA copies **RAM → VRAM** only.

### Source address (CPU address space)

* `0x30` — DMA_SRC_LO (R/W)
* `0x31` — DMA_SRC_HI (R/W)

### Destination address (VRAM)

* `0x32` — DMA_DST_LO (R/W)
* `0x33` — DMA_DST_HI (R/W)

### Length

* `0x34` — DMA_LEN_LO (R/W)
* `0x35` — DMA_LEN_HI (R/W)

### `0x36` — DMA_CTRL (R/W)

* Bit 0: START
* Bit 1: BUSY (read-only reflection)
* Bit 2: QUEUE_IF_NOT_VBLANK (policy bit; recommended default = 1)
* Other bits reserved

### DMA Execution Rules (Mandatory)

* If START written while in VBlank:

  * Perform copy immediately
  * BUSY clears immediately
  * Optional: raise DMA_DONE flag in IRQ_STATUS
* If START written outside VBlank:

  * If QUEUE_IF_NOT_VBLANK=1: queue it for next VBlank
  * Else: ignore and optionally set DMA_ERROR flag

---

## 6. Controller Input (`0x40–0x4F`)

### `0x40` — PAD1 (R)

Bitfield (suggested mapping):

* Bit 0–3: D-pad (Up/Down/Left/Right)
* Bit 4–7: Buttons 1–4

### `0x41` — PAD1_SYS (R)

* Start / Select
* Possibly hardware ID bits

### `0x42` — PAD2 (R)

Same as PAD1

### `0x43` — PAD2_SYS (R)

Same as PAD1_SYS

### `0x44–0x4F`

Reserved (multitap, light gun, etc.)

---

## 7. Expansion / Reserved (`0x50–0x5F`)

Reserved for:

* Light gun support
* Future peripherals
  Default behavior:
* Read `0xFF`, writes ignored

---

## 8. PSG (`0x60–0x6F`)

SN76489-style PSG typically uses a single write port.

* `0x60` — PSG_DATA (W)
  Write-only latch/data port.

Optional:

* `0x61` — PSG_STATUS (R) (not required for SN76489; keep reserved)

Remaining:

* `0x62–0x6F` reserved

---

## 9. YM2151 + PCM (`0x70–0x7F`)

### YM2151 (typical two-port model)

* `0x70` — OPM_ADDR (W)
* `0x71` — OPM_DATA (W/R optional)
  If reads are unsupported in chosen core, return `0xFF`.

### PCM (2 channels, trigger-based)

For each channel: sample start, length, volume, trigger.

#### Channel 0

* `0x72` — PCM0_START_LO (R/W)
* `0x73` — PCM0_START_HI (R/W)
* `0x74` — PCM0_LEN (R/W)
* `0x75` — PCM0_VOL (R/W)
* `0x76` — PCM0_CTRL (R/W)

  * Bit 0: TRIGGER (one-shot)
  * Bit 1: LOOP (optional; default off)
  * Bit 7: BUSY (read-only reflection)

#### Channel 1

* `0x77` — PCM1_START_LO (R/W)
* `0x78` — PCM1_START_HI (R/W)
* `0x79` — PCM1_LEN (R/W)
* `0x7A` — PCM1_VOL (R/W)
* `0x7B` — PCM1_CTRL (R/W)

#### Mixer / Master

* `0x7C` — AUDIO_MASTER_VOL (R/W)
* `0x7D` — AUDIO_PAN (R/W) *(optional)*
* `0x7E–0x7F` reserved

---

## 10. Timer / System IRQ (`0x80–0x8F`)

### `0x80` — IRQ_STATUS (R)

Latched interrupt sources (read-only).

* Bits (skeleton):

  * Bit 0: VBLANK pending
  * Bit 1: TIMER pending
  * Bit 2: SCANLINE pending
  * Bit 3: SPR_OVERFLOW pending
  * Bit 4: DMA_DONE pending (optional)
  * Others reserved

### `0x81` — IRQ_ENABLE (R/W)

Mask which sources can assert `/INT`.

### `0x82` — IRQ_ACK (W)

Write-1-to-clear for IRQ_STATUS bits.

* Example: write `0b0000_0001` clears VBlank pending.

### Timer

* `0x83` — TIMER_RELOAD_LO (R/W)
* `0x84` — TIMER_RELOAD_HI (R/W)
* `0x85` — TIMER_CTRL (R/W)

  * enable
  * prescaler select (TBD)
  * one-shot vs periodic

### Scanline compare

* `0x86` — SCANLINE_CMP (R/W)
  When beam reaches this scanline, set SCANLINE pending (if enabled).

### `0x87–0x8F` reserved

---

## 11. Debug / Reserved (`0x90–0x9F`)

This range is reserved. Optional emulator-only features can be exposed here **only** if we intentionally allow “non-hardware” debug ports.

Default:

* Reads `0xFF`, writes ignored

---

## 12. Register Behavior Guarantees (Implementation Contract)

1. **Status bits are latched** and cleared only by `IRQ_ACK` (write-1-to-clear).
2. **VBlank flag** is true only during scanlines 192–261.
3. **DMA** is legal only during VBlank; policy for non-VBlank triggers is defined by DMA_CTRL queue bit.
4. **Scanline IRQ** fires at scanline boundary (start of line).
5. Unmapped I/O returns `0xFF`.

---

## 13. What This Skeleton Enables Immediately

With this map, we can implement:

* Bus + I/O dispatch
* IRQ controller
* VBlank scheduling
* DMA engine
* Stubbed PPU register storage
* Stubbed APU register storage
* Cartridge banking control

---
