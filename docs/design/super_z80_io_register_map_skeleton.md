# Super_Z80 I/O Register Map

## Canonical, Emulator-Aligned Specification (v1.4)

**Status:** Canonical hardware contract
**Audience:** Emulator core + ROM authors
**Design intent:** Stable, deterministic, arcade-authentic register interface

---

## 0. Global Rules (Locked)

### I/O model

* Z80 8-bit I/O ports
* Ports **0x00–0x9F** are canonical
* Upper address byte may mirror (implementation detail)

### Unmapped ports

* Reads return `0xFF`
* Writes are ignored

### Timing guarantees

* VBlank = scanlines **192–261**
* DMA executes **only** during VBlank
* Register writes take effect at **scanline boundaries**, never mid-scanline
* IRQ sources are latched until explicitly acknowledged

---

## 1. Port Range Summary

| Range     | Subsystem                   |
| --------- | --------------------------- |
| 0x00–0x0F | Cartridge / mapper          |
| 0x10–0x1F | Video control + palette     |
| 0x20–0x2F | Sprite system               |
| 0x30–0x3F | DMA engine                  |
| 0x40–0x4F | Controller input            |
| 0x50–0x5F | Reserved                    |
| 0x60–0x6F | PSG                         |
| 0x70–0x7F | YM2151 + PCM                |
| 0x80–0x8F | Timer + IRQ                 |
| 0x90–0x9F | Reserved / debug (optional) |

---

## 2. Cartridge / Mapper (0x00–0x0F)

### 0x00 — MAP_CTRL (R/W)

Mapper control flags (implementation-defined).

### 0x01 — ROM_BANK_0 (R/W)

Selects ROM bank for `0x4000–0x7FFF`.

### Reset rule (mandatory)

* Bank 0 mapped at reset
* Mapper returns to default state

---

## 3. Video Control (0x10–0x1F)

### 0x10 — VDP_STATUS (R)

Latched status bits:

| Bit    | Meaning              |
| ------ | -------------------- |
| 0      | VBLANK active        |
| 1      | SPR_OVERFLOW latched |
| 2      | SCANLINE_IRQ pending |
| others | reserved             |

Reading **does not clear** flags.

---

### 0x11 — VDP_CTRL (R/W)

**Locked bit layout:**

| Bit | Meaning                                         |
| --- | ----------------------------------------------- |
| 7   | Display enable                                  |
| 1   | Plane B enable                                  |
| 0   | Plane A enable (forced on when display enabled) |

> Sprite enable is **not** controlled here. See `SPR_CTRL`.

Writes take effect at **next scanline boundary**.

---

### Plane A scroll

* 0x12 — PLANE_A_SCROLL_X
* 0x13 — PLANE_A_SCROLL_Y

### Plane B scroll

* 0x14 — PLANE_B_SCROLL_X
* 0x15 — PLANE_B_SCROLL_Y

---

### Tilemap / Pattern base (page-based)

| Port | Function     |
| ---- | ------------ |
| 0x16 | PLANE_A_BASE |
| 0x17 | PLANE_B_BASE |
| 0x18 | PATTERN_BASE |

* Units: **1 KB pages**
* Lower address bits implicitly zero

---

### Palette access (canonical)

#### 0x1E — PAL_ADDR (R/W)

* **Byte-addressed**
* Valid range: 0x00–0xFF
* Auto-increment on PAL_DATA access

#### 0x1F — PAL_DATA (R/W)

**Palette format (locked):**

* 128 entries
* Each entry = **9-bit RGB**
* Stored across **two bytes**

```
Bits 8–6: Red
Bits 5–3: Green
Bits 2–0: Blue
```

Palette updates become visible at **next scanline boundary**.

---

### ❌ VRAM access ports REMOVED

The Super_Z80 **does not expose CPU-accessible VRAM ports**.

All VRAM manipulation occurs via:

* DMA
* Internal PPU fetch logic

This is intentional and arcade-authentic.

---

## 4. Sprite System (0x20–0x2F)

### 0x20 — SPR_CTRL (R/W)

| Bit | Meaning                               |
| --- | ------------------------------------- |
| 7   | Sprite enable                         |
| 1–0 | Size mode (00 = 8×8; others reserved) |

---

### 0x21 — SAT_BASE (R/W)

VRAM page index of Sprite Attribute Table.

---

### 0x22 — SPR_STATUS (R)

| Bit    | Meaning          |
| ------ | ---------------- |
| 0      | OVERFLOW latched |
| others | reserved         |

---

## 5. DMA Engine (0x30–0x3F)

### Source (CPU space)

* 0x30 — DMA_SRC_LO
* 0x31 — DMA_SRC_HI

### Destination (video bus)

* 0x32 — DMA_DST_LO
* 0x33 — DMA_DST_HI

### Length

* 0x34 — DMA_LEN_LO
* 0x35 — DMA_LEN_HI

---

### 0x36 — DMA_CTRL (R/W) **(Locked)**

| Bit | Name                | Description                 |
| --- | ------------------- | --------------------------- |
| 7   | BUSY (R)            | DMA in progress             |
| 3   | DST_IS_PALETTE      | 0=VRAM, 1=Palette RAM       |
| 1   | QUEUE_IF_NOT_VBLANK | Queue DMA until next VBlank |
| 0   | START (W)           | Initiate DMA                |

**Rules:**

* DMA executes only during VBlank
* If queued, runs at next VBlank start
* No CPU stall

---

## 6. Controller Input (0x40–0x4F)

### 0x40 — PAD1 (R)

Bits:

* 0–3: D-pad
* 4–7: Buttons 1–4

### 0x41 — PAD1_SYS (R)

Start / Select

### 0x42 / 0x43 — PAD2 equivalents

---

## 7. PSG (0x60–0x6F)

### 0x60 — PSG_DATA (W)

SN76489-style write-only latch.

---

## 8. YM2151 + PCM (0x70–0x7F)

### YM2151

* 0x70 — OPM_ADDR (W)
* 0x71 — OPM_DATA (W; reads return 0xFF)

---

### PCM Channel 0

* 0x72 — START_LO
* 0x73 — START_HI
* 0x74 — LEN
* 0x75 — VOL
* 0x76 — CTRL

### PCM Channel 1

* 0x77–0x7B mirror Channel 0

CTRL bits:

* Bit 0: TRIGGER
* Bit 7: BUSY (R)

---

## 9. Timer / IRQ (0x80–0x8F)

### 0x80 — IRQ_STATUS (R)

Latched pending flags.

### 0x81 — IRQ_ENABLE (R/W)

Mask bits.

### 0x82 — IRQ_ACK (W)

Write-1-to-clear.

### Timer

* 0x83 — RELOAD_LO
* 0x84 — RELOAD_HI
* 0x85 — TIMER_CTRL

### Scanline compare

* 0x86 — SCANLINE_CMP

---

## 10. Reserved / Debug (0x90–0x9F)

* Reads return 0xFF
* Writes ignored
* Emulator-only debug ports may exist **only if explicitly enabled**

---

## Final Outcome

With this revision:

* ❌ No undocumented features remain
* ❌ No CPU-visible VRAM fiction
* ✅ DMA → Palette is first-class, not a hack
* ✅ Palette + tile formats are locked
* ✅ Demo ROM authors now have a **real hardware contract**

---

