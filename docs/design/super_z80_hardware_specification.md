# Super_Z80 Premium Console

## Hardware Design Specification (v1.3 – Locked)

**Design Window:** 1985–1987 (historically plausible high-end home release)
**Philosophy:** Z80-based *arcade-at-home* premium console, positioned as a mid-1980s conceptual ancestor to the Neo Geo AES. It delivers mid-tier arcade audiovisual quality (Sega System 1/2 class) in cartridge form, optimized for home use with strong sprites, true parallax scrolling, FM audio, and generous cartridge capacity — clearly above NES and Sega Master System, while remaining firmly 8-bit.
**Primary Goal:** Arcade-style fidelity at home; premium pricing tier (~$400–500 console, ~$60–100 cartridges).

---

## 1. System Identity

* Premium 8-bit video game console (not a computer)
* No keyboard, disk, tape, or operating system
* Cartridge-only software distribution
* Large arcade-style controllers (8-way joystick + 4 buttons)
* NTSC-only output

---

## 2. CPU Subsystem

### CPU

* **Z80H** (8 MHz-rated variant) running at **5.369 MHz**

    * Clock derived from NTSC colorburst crystal (21.477 MHz ÷ 4)
    * ~1.5× performance over typical Z80A home consoles

### Architecture

* Single main CPU handles game logic, video control, and audio sequencing
* No secondary sound CPU (cost-optimized, arcade-proven approach)

### Interrupts

* Interrupt Mode 1 (IM 1)
* Interrupt sources:

    * Vertical Blank (primary)
    * Programmable system timer
    * Optional scanline compare or sprite overflow flag

### Reset Behavior

* Reset vector at `0x0000`
* Interrupts disabled on reset
* Cartridge ROM Bank 0 mapped at reset

---

## 3. Memory Architecture

### Z80 Logical Address Space (64 KB)

| Address Range   | Function                                 |
| --------------- | ---------------------------------------- |
| `0x0000–0x7FFF` | Cartridge ROM (fixed + switchable banks) |
| `0x8000–0xBFFF` | Video RAM window                         |
| `0xC000–0xFFFF` | Work RAM (fixed)                         |

### Physical Memory

* **Work RAM:** **32 KB total**, simplified mapping

    * 16 KB fixed at `0xC000–0xFFFF`
    * Remaining RAM mapped via a simple fixed or single-bit selectable window
* **Video RAM (VRAM):** **48 KB total**, shared between tile planes and sprites
* **Cartridge ROM:** 128 KB – 1024 KB, banked
* **Cartridge SRAM:** Optional 8 KB battery-backed

### Banking

* Cartridge ROM bank switching via I/O registers
* No complex multi-page Work RAM banking

---

## 4. Cartridge Format

### Cartridge Header (Bank 0)

* Magic identifier: `"SZ80"`
* Hardware revision
* Mapper type
* Entry point address
* ROM size
* RAM size
* Region (NTSC)
* Feature flags (FM, PCM, save RAM, etc.)

### Boot Model

* No system BIOS
* Cartridge software initializes all hardware

---

## 5. Video Subsystem (Arcade-Class)

**Architectural Model:**
Dedicated multi-block video system inspired by mid-80s arcade boards.

```
Z80
 ├─ Tile Generator A (Background Plane)
 ├─ Tile Generator B (Parallax / Foreground Plane)
 ├─ Sprite Generator
 ├─ Palette RAM + RGB DAC
 └─ Priority / Mixer
```

### Display Timing

* NTSC-only
* **Active resolution: 256×192**
* Refresh rate: 60 Hz
* VBlank interrupt once per frame
* Optional scanline compare interrupt

### Background Tile Planes (A & B)

* Tile size: 8×8 pixels
* Color depth: 4 bpp (16 colors per tile)
* Tilemap size: **32×24 tiles**
* **Independent fine X/Y scrolling per plane**
* Attributes:

    * Palette select
    * Priority
    * Horizontal / Vertical flip
* VRAM allocation from shared 48 KB pool

Dual planes guarantee true hardware parallax scrolling without CPU overhead.

### Window / HUD

* Fixed HUD achievable via scanline split or window registers

### Sprite System

| Feature       | Specification                               |
| ------------- | ------------------------------------------- |
| Total sprites | **48**                                      |
| Per scanline  | **16**                                      |
| Sizes         | 8×8, 8×16, 16×16 (implementation-dependent) |
| Attributes    | Palette select, priority, H/V flip          |
| VRAM usage    | Shared pool                                 |

Sprite evaluation and scanline limits are hardware-managed to minimize flicker.

### Color System

* **128 palette entries**
* **9-bit RGB output** (3 bits per channel)
* Per-tile and per-sprite palette selection
* Mid-frame palette changes supported

### DMA Engine

* **VBlank-only RAM → VRAM DMA**
* Used for tile, sprite, and map updates
* No VRAM → VRAM block copy hardware

---

## 6. Audio Subsystem

### Components (Locked)

The Super_Z80 audio system consists of two hardware sound generators:

* **PSG:** SN76489-compatible programmable sound generator
  * 3 tone channels
  * 1 noise channel
* **FM Synthesizer:** YM2151 (OPM)
  * 8 independent FM channels

No PCM or sample playback hardware is present.

---

### Architectural Model

Z80
├─ PSG (SN76489-compatible)
├─ YM2151 (OPM)
└─ Stereo mixer + DAC

Audio mixing is performed in hardware.  
The CPU sequences audio exclusively through register writes.

---

### Design Rationale

This configuration represents a **premium home-console audio system** for the mid-1980s:

* PSG provides inexpensive effects, noise, and simple tonal layers
* YM2151 provides rich, arcade-class FM music
* Absence of PCM avoids sample-driven content expectations typical of later arcade and 16-bit systems

This balance intentionally exceeds NES/SMS-class audio while remaining plausible for a high-end consumer console.

---

### Timing Rules

* Audio chips advance based on elapsed system time
* Audio stepping is **time-driven**, not frame-driven
* No audio-generated interrupts exist

---

## 7. Input Subsystem

* Two controller ports
* 8-way digital D-Pad
* Four action buttons
* Start / Select
* Polled via I/O ports
* Optional expansion / light gun support

---

## 8. Timer & System Services

* Programmable interval timer
* Periodic interrupts for gameplay timing and audio sequencing
* Status and acknowledge registers

---

## 9. I/O Port Map (High-Level)

| Port Range  | Function                        |
| ----------- | ------------------------------- |
| `0x00–0x1F` | Memory mapper & video registers |
| `0x20–0x3F` | Sprite system & DMA control     |
| `0x40–0x5F` | Controller input                |
| `0x60–0x7F` | Audio (PSG / YM2151 / PCM)      |
| `0x80–0x9F` | Timer & system control          |

Exact register definitions deferred to implementation phase.

---

## 10. Locked Decisions Summary

* NTSC-only, 256×192 active resolution
* **Dual independently scrolling background planes**
* Z80H @ 5.369 MHz (single CPU)
* 48 KB VRAM
* 32 KB Work RAM
* 48 sprites total, 16 per scanline
* YM2151 FM synthesis + PSG + 2-channel PCM
* VBlank-only RAM → VRAM DMA
* Optional cartridge save RAM
* Cartridge-only, no BIOS

---

**Super_Z80** represents a historically plausible, premium mid-1980s console — a true *arcade board in a shell* — establishing a clear conceptual lineage toward what the Neo Geo would later become.
