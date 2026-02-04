# Super_Z80 Project Glossary (Authoritative)

**Status:** Active
**Scope:** Hardware specification, emulator implementation, debugging, and documentation
**Purpose:** Ensure consistent terminology across all Super_Z80 discussions and code

If a term appears in code, comments, or discussion and is defined here, **this definition applies**.

---

## A

### **Accumulator (Timing)**

A fractional counter used to distribute non-integer CPU cycles evenly across scanlines.
Used to ensure long-term timing accuracy when `CPU_HZ / LINE_HZ` is not an integer.

---

## B

### **Background Plane**

A tile-based scrolling layer rendered by the video system.

* Super_Z80 has **two** background planes:

  * Plane A (primary background)
  * Plane B (parallax / foreground)
* Each plane has independent X/Y scrolling and attributes.

---

### **Bus**

The abstracted communication layer through which the CPU accesses all memory and I/O.

* All CPU reads/writes pass through the bus
* The bus dispatches requests to:

  * Work RAM
  * Cartridge ROM/SRAM
  * VRAM window
  * I/O registers
* Mirrors the real hardware address and data buses

---

## C

### **Cartridge**

The physical game medium containing:

* Program ROM
* Optional battery-backed SRAM
* Header metadata
* Mapper logic

In the emulator, this is represented by the **Cartridge subsystem**.

---

### **Cycle (CPU Cycle / T-State)**

The smallest unit of time for the Z80 CPU.

* One T-state = one tick of the CPU clock
* Instruction timing is measured in T-states
* All CPU scheduling is expressed in T-states

---

## D

### **DMA (Direct Memory Access)**

A hardware mechanism that copies data from system RAM to VRAM without CPU intervention.

* Super_Z80 DMA:

  * Direction: RAM → VRAM only
  * Legal only during VBlank
  * Instantaneous in emulation
* DMA is **never** allowed during visible scanlines

---

## F

### **Frame**

One complete video refresh cycle.

* Consists of **262 scanlines**
* Includes visible and VBlank periods
* Frame rate is derived from the scanline rate (~60.098 Hz)

A frame boundary occurs when scanline 261 completes and scanline 0 begins.

---

## I

### **IM 1 (Interrupt Mode 1)**

The Z80 interrupt mode used by Super_Z80.

* All interrupts vector to address `0x0038`
* Interrupt acknowledge takes 13 T-states
* Interrupt enable behavior follows Z80 rules (EI delay applies)

---

### **IRQ (Interrupt Request)**

A signal asserting that the CPU should service an interrupt.

* Super_Z80 uses a **wired-OR IRQ line**
* Multiple sources may assert simultaneously
* Software must clear the relevant status flags

---

## L

### **Line Rate**

The frequency at which video scanlines are processed.

* NTSC standard: **15,734.264 Hz**
* Used as the fundamental scheduling cadence for the emulator

---

## P

### **Palette**

A table of color entries used by tiles and sprites.

* Super_Z80 palette:

  * 128 entries
  * 9-bit RGB (3 bits per channel)
* Tiles and sprites reference palette indices, not raw colors

---

### **Plane**

See **Background Plane**.

---

### **PPU (Picture Processing Unit)**

Shorthand for the entire video subsystem.

Includes:

* Tile generators
* Sprite engine
* Palette RAM
* Priority mixer
* Scanline timing logic

---

## S

### **Scanline**

A single horizontal line of the video beam.

* Super_Z80 scanlines:

  * 0–191: visible
  * 192–261: VBlank
* The emulator advances time **one scanline at a time**

Scanlines are the **primary scheduling unit**.

---

### **Scheduler**

The emulator logic that coordinates CPU, video, audio, and DMA advancement.

* Operates on scanlines
* Distributes CPU cycles via a fractional accumulator
* Ensures deterministic ordering of events

---

### **Sprite**

A movable graphical object rendered on top of background planes.

* Total sprites: 48
* Per scanline limit: 16
* Attributes include:

  * Position
  * Tile index
  * Palette
  * Priority
  * Flip flags

---

## T

### **T-State**

See **Cycle**.

---

### **Timing Model**

The authoritative definition of how time advances in the emulator.

Defined in:

* `super_z80_emulation_timing_model.md`

All subsystem behavior must conform to this model.

---

## V

### **VBlank (Vertical Blank)**

The non-visible period between frames.

* Begins at scanline **192**
* Ends at scanline **261**
* Only time when:

  * DMA is allowed
  * VRAM updates are guaranteed not to affect visible output
* VBlank IRQ is asserted at its start

---

### **VRAM (Video RAM)**

Memory used by the video subsystem.

Contains:

* Tile pattern data
* Tilemaps
* Sprite attribute tables

Access rules:

* CPU accesses VRAM via a windowed mapping
* Direct writes outside VBlank may be ignored or undefined

---

## W

### **Work RAM**

General-purpose system RAM used by the CPU.

* 32 KB total
* Fixed mapping at `0xC000–0xFFFF`
* Remaining RAM accessible via fixed or simple bank window

---

## Z

### **Z80H**

The high-speed variant of the Z80 CPU.

* Rated for higher clock speeds
* Used in Super_Z80 at ~5.369 MHz
* Behavior identical to standard Z80 from a software perspective

---

## Glossary Usage Rule

* If a term appears in:

  * Code
  * Comments
  * Design documents
  * Debug UI labels

…its meaning **must match this glossary**.

If a new term is introduced, it must be added here.

---

### Status

**Active and binding for the Super_Z80 project unless explicitly revised.**

---
