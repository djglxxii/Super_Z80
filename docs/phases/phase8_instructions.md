## CODING-AGENT PROMPT (Phase 8 — Palette & Mid-Frame Behavior)

### Scope / Constraints (must enforce)

* Implement **palette RAM + palette I/O + palette visibility timing**, integrated with existing **scanline-driven renderer** (Plane A only).
* **Do not** implement Plane B, sprites, window/HUD split features, new IRQ sources, or APU.
* VBlank IRQ remains the only IRQ source (already exists from earlier phases).
* DMA from Phase 6 exists; you may extend it only as required to satisfy palette DMA requirements.

---

# 1) Palette RAM (PPU-owned): exact internal storage + encoding decisions

### 1.1 Capacity and canonical format

* Palette RAM has **128 entries** (index 0–127).
* Each entry is **9-bit RGB, 3 bits per channel (R3:G3:B3)**.

### 1.2 Internal storage choice (mandatory decision)

Implement **two copies**:

* `staged_pal[128]` (write target)
* `active_pal[128]` (render target)

Store each entry as a **uint16_t packed color** in this exact bit layout:

* Bits **0–2**: R (0–7)
* Bits **3–5**: G (0–7)
* Bits **6–8**: B (0–7)
* Bits **9–15**: **0** (read as 0 internally)

This choice is consistent with “stored packed across bytes (exact packing TBD)” while staying simple and deterministic.

### 1.3 Expansion for framebuffer (renderer convenience)

When rendering, expand `active_pal[i]` to 8-bit-per-channel RGB for the SDL framebuffer:

* `r8 = (r3 * 255) / 7`
* `g8 = (g3 * 255) / 7`
* `b8 = (b3 * 255) / 7`
  Cache optional: you may precompute `active_rgb888[128]` at commit time (scanline boundary) to avoid per-pixel expansion.

---

# 2) Palette I/O ports (0x1E/0x1F): behavior, unmapped bits, auto-increment

The I/O skeleton defines:

* `0x1E` — **PAL_ADDR** (R/W)
* `0x1F` — **PAL_DATA** (R/W)

### 2.1 PAL_ADDR model (exact)

Define `pal_addr` as an **8-bit byte address** into a 256-byte palette aperture:

* Palette entry `n` occupies 2 bytes:

    * byte address `n*2 + 0` = low byte
    * byte address `n*2 + 1` = high byte

Thus:

* `pal_index = pal_addr >> 1` (0–127)
* `pal_byte_sel = pal_addr & 1` (0=low, 1=high)

**PAL_ADDR read** returns the current `pal_addr` (0–255).
No other status bits are exposed in Phase 8.

### 2.2 PAL_DATA encoding (exact)

PAL_DATA reads/writes one byte at the current `pal_addr`.

For palette entry packed as described in §1.2:

* **Low byte** (`pal_byte_sel=0`): bits 0–7 of packed color (contains R and G and part of B)
* **High byte** (`pal_byte_sel=1`): bits 8–15 of packed color (only bit0 used for B2; the rest are 0 internally)

Practical effect:

* Only **bit 0** of the high byte is used (because packed bit 8 = B2).
* On reads: return the real stored byte (high byte will typically be `0x00` or `0x01` depending on B2).
* On writes: accept all 8 bits, but only the meaningful bits affect the 9-bit value; upper unused bits in the 16-bit packed value are ignored/cleared.

### 2.3 Auto-increment policy (mandatory decision)

Implement **auto-increment by +1 byte** after every PAL_DATA read or write.

* Rationale: matches common VDP-style register streaming and keeps palette DMA/simple loops efficient.
* Documented as “if you choose (document it)”—we are choosing yes.

### 2.4 Unmapped bits / open bus rules

* If PAL_ADDR is read, return the 8-bit `pal_addr` (fully defined).
* If PAL_DATA is read, return the addressed byte (fully defined).
* For any reserved/unimplemented palette-related bits beyond this design (none in Phase 8), follow the project rule: **unmapped reads return 0xFF; unmapped writes ignored** (same convention used elsewhere).

---

# 3) Palette DMA support (required by diagnostic ROM; choose approach and document)

The diagnostic ROM explicitly requires **palette initialization via DMA** during VBlank and Phase 8 checklist expects “Palette DMA works”.

## Decision: extend existing DMA engine to target Palette RAM

Even though earlier DMA text focuses on RAM→VRAM, Phase 8 must pass the diagnostic behavior. Implement a **destination target selector** without adding new IRQ sources:

### 3.1 DMA target selector (minimal change)

Add one new bit in DMA control (or reuse a previously reserved bit) to select destination:

* `DMA_CTRL bit 3: DST_IS_PALETTE`

    * 0 = destination is VRAM (existing behavior)
    * 1 = destination is Palette RAM (new Phase 8 behavior)

Do not add new registers; keep it a strict extension.

### 3.2 DMA semantics for Palette RAM

When `DST_IS_PALETTE=1` and DMA executes (VBlank-only policy remains):

* Interpret `DMA_DST` as an 8-bit **palette byte address** (0–255) like `pal_addr`.

    * Only the low 8 bits are used; higher bits ignored.
* Copy `DMA_LEN` bytes from CPU RAM to palette aperture:

    * For each byte copied:

        * write it to palette via the same logic as PAL_DATA write to `pal_addr`
        * increment palette byte address by 1 (wrap 0xFF→0x00)
* Writes go to **staged_pal** (not active) to preserve the timing rule.

### 3.3 VBlank-only execution remains mandatory

Diagnostic ROM states DMA during VBlank only and failure if palette writes take effect mid-frame.

* If DMA is requested outside VBlank, keep Phase 6 policy (queue or ignore), but **do not allow visible mid-scanline palette changes**.

---

# 4) Palette timing / visibility rule (mandatory): no mid-scanline tearing

Phase 8 checklist requires:

* “Palette changes visible only at correct boundary”
* “Mid-frame palette changes affect next scanline (not earlier)”
* “No tearing or partial updates”.

The I/O skeleton recommends next scanline boundary.

## Decision: **Palette writes become visible at START OF NEXT SCANLINE**

Implement the staged/active model:

* All PAL_DATA writes and Palette-DMA writes update **staged_pal** immediately.
* `active_pal` is only updated by a **commit** at a deterministic boundary:

    * **At the start of each scanline** (scanline boundary), copy `staged_pal → active_pal`.
    * Then render that scanline using `active_pal` (or a per-scanline snapshot derived from it).

### 4.1 Exact commit point in scanline scheduler

At the start of scanline `S`:

1. Commit palette (`active = staged`)
2. Render scanline `S` (if visible) using active palette snapshot
3. Run other scanline tasks already defined by Phase 7/Phase 6 ordering (do not reorder time ownership; scheduler owns time)

This guarantees:

* Any palette update occurring during scanline `S` cannot affect scanline `S` output (no tearing).
* It will affect scanline `S+1` (next scanline), satisfying checklist language.

### 4.2 Tracking metadata for debug

Maintain:

* `last_pal_write_frame`, `last_pal_write_scanline`
* `last_pal_commit_frame`, `last_pal_commit_scanline`
* A per-entry dirty bit for “modified this frame” highlighting.

---

# 5) Rendering integration (Plane A only): active palette usage per scanline

Phase 7 requires “Palette indexing correct” and “Scanline renderer produces stable output”. Phase 8 adds deterministic palette boundaries.

### 5.1 Rule

Plane A renderer must map tile pixel indices → RGB through the **ACTIVE palette snapshot for that scanline**.

### 5.2 Implementation notes (no full code; only interface expectations)

* Renderer should never consult `staged_pal` directly.
* If you cache expanded colors, only cache from `active_pal` at commit time.
* Ensure that the palette index coming from tile pixels (0–15 within a palette group) resolves to the final 0–127 entry exactly as Phase 7 already established (do not change indexing rules here; just change where color is sourced).

---

# 6) Diagnostic-style test ROM behavior (describe only; do not fully code)

Create a minimal test ROM aligned with the diagnostic cartridge expectations:

### 6.1 Initialization

* Set up Plane A only; disable Plane B and sprites (matches diagnostic spec).
* Prepare palette bytes in RAM at `0xC100–0xC11F` (or equivalent region), with at least:

    * entry 0 black
    * entry 1 red
    * entry 2 green
    * entry 3 blue
    * entry 4 white

### 6.2 Palette load via DMA during VBlank

* During VBlank only, trigger DMA RAM→Palette RAM for “number of palette bytes”.
* Confirm in emulator debug that DMA is rejected/queued outside VBlank (existing policy), but does not cause visible tearing.

### 6.3 Tile rendering

* Display tiles that use palette entries 1–4 across the screen (static pattern is fine).

### 6.4 VBlank ISR palette pulse

In the IM1 ISR at `0x0038`, do:

1. Increment ISR count variables
2. Toggle **palette entry 1** between red and blue
3. Write to IRQ_ACK to clear VBlank pending
4. RETI

### 6.5 Expected output

* Entire screen (or a large region) visibly changes color **once per frame** with **no tearing**.
* This must remain stable indefinitely.

---

# 7) Debug visibility (mandatory panels / fields)

Add a palette debug view consistent with the project’s “debug visibility required” ethos:

### 7.1 Palette viewer panel

* List entries 0–127 with:

    * packed 16-bit value
    * decoded R3/G3/B3
    * rendered swatch (RGB888)
* Toggle display of **Active** vs **Staged** palette (side-by-side or switch).

### 7.2 Live registers/state

* Current `PAL_ADDR` value (0–255), derived `index` + `byte_sel`.
* Last palette write:

    * frame #
    * scanline #
    * which entry/byte was written
* Last commit boundary:

    * frame #
    * scanline #
* Optional: highlight entries modified this frame (dirty bits).

(Advisory support: the design PDF discusses palette updates applied for the next scanline in a scanline-based renderer, matching this phase’s intent.)

---

# 8) Phase 8 pass/fail criteria mapped to Bring-Up Checklist

Map outcomes directly to Phase 8 checklist items:

### PASS if all are true

1. **Palette DMA works**

    * Diagnostic palette init uses DMA RAM→Palette RAM during VBlank and produces correct colors.
2. **Palette changes visible only at correct boundary**

    * Writes during scanline S never affect scanline S; they affect scanline S+1 (start-of-next-scanline commit).
3. **Mid-frame palette changes affect next scanline (not earlier)**

    * Verified by instrumentation: palette write scanline vs first visible scanline affected.
4. **No tearing or partial updates**

    * No mixed palette within a single scanline; full scanline uses a single active palette snapshot.
5. Diagnostic behavior readiness: “palette pulses once per frame” (Phase 9 checklist line item) is achievable with this implementation.

### FAIL if any occur

* Any palette update becomes visible **mid-scanline** (tearing).
* DMA palette load occurs outside VBlank or affects visible scanlines contrary to policy.
* Palette pulse occurs more than once per frame or misses frames (timing/IRQ handling regression).

---

## Required interface skeletons (no full implementations)

### PPU (video) module

* `class PPU {`

    * `uint16_t staged_pal[128];`
    * `uint16_t active_pal[128];`
    * `uint32_t active_rgb888[128]; // optional cache`
    * `uint8_t pal_addr;`
    * `void pal_commit_at_scanline_start(int frame, int scanline);`
    * `uint8_t io_read(uint8_t port);   // handles 0x1E/0x1F`
    * `void io_write(uint8_t port, uint8_t value);`
    * `void render_scanline(int frame, int scanline, uint32_t* out_row_rgb888);`
* `};`

### DMA engine extension (minimal)

* Add `DST_IS_PALETTE` bit in DMA_CTRL.
* When executing DMA during VBlank:

    * if `DST_IS_PALETTE=0`: existing RAM→VRAM path
    * if `DST_IS_PALETTE=1`: RAM→Palette aperture path (writes into PPU staged palette via a method like `ppu.palette_write_byte(addr, value)`)

### Scheduler hook point

* At scanline start, call `ppu.pal_commit_at_scanline_start(frame, scanline)` before rendering that scanline.

---
