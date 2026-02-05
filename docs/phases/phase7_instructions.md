## CODING-AGENT PROMPT: Super_Z80 Phase 7 — PPU Bring-Up (Plane A Only)

### Goal

Implement a minimal but correct **Plane A** renderer that produces a **256×192 framebuffer**, updated **one scanline at a time**, using **VRAM-backed 8×8 tiles (4bpp)** and a **32×24 tilemap**. Present the framebuffer once per frame via the existing SDL path.

### Strict scope (do not implement)

* No Plane B, no sprites, no window/HUD split, no mid-frame palette tricks (Phase 8), no audio.
* DMA from Phase 6 is assumed present and remains the primary way to load VRAM.
* IRQs remain VBlank-only (already implemented in Phase 5); do not add new IRQ sources.
* No “render whole frame from final VRAM state” shortcuts—render must be scanline-driven.

---

## 1) Minimal PPU Data Model (Phase 7)

### Backing memories

* **VRAM**: byte array (already exists from Phase 6). Total VRAM = **48 KB**.
* **Palette RAM**:

    * Phase 7 minimum: you may start with a fixed palette table if palette ports aren’t fully implemented yet.
    * Note future requirement: 128 entries, **9-bit RGB (3-3-3)**.

### Required registers (CPU-visible I/O)

Implement these ports with the semantics below; keep bitfields minimal and forward-compatible:

**`0x10` — VDP_STATUS (R)**

* Must expose at least `VBLANK` bit = 1 during scanlines 192–261. (Already wired per Phase 5; do not regress.)

**`0x11` — VDP_CTRL (R/W)**

* Minimum bit: **Display Enable** (gate rendering; if off, output black).
* Writes take effect at the **next scanline boundary** (Phase 7 timing rule).

**`0x12` — PLANE_A_SCROLL_X (R/W)** (fine scroll, pixels)
**`0x13` — PLANE_A_SCROLL_Y (R/W)** (fine scroll, pixels)

**`0x16` — PLANE_A_BASE (R/W)** (tilemap base selector, VRAM)
**`0x18` — PATTERN_BASE (R/W)** (pattern base selector, VRAM)

* These select VRAM base addresses in implementation-defined units (recommend: 1KB pages); store as page indices.

> Note: VRAM/palette ports (`0x1A–0x1F`) may already exist or be partial; Phase 7 does not require full indirect VRAM writes if DMA covers the test. Keep ports reserved/compatible.

---

## 2) Tile Format and Decode (deterministic; easy to revise)

### Ground truth constraints

* Tiles are **8×8**
* **4bpp indexed** (16 colors per tile)

### Assumption (because packing is not explicitly specified in the hardware spec)

Use a **packed 4bpp** format (simple and common):

* Each tile row is 8 pixels = **4 bytes per row** (2 pixels per byte: high nibble = left pixel, low nibble = right pixel).
* Tile size = 8 rows × 4 bytes = **32 bytes per tile**.
* Tile `t` pattern address:

    * `pattern_base_addr = PATTERN_BASE * 1024`
    * `tile_addr = pattern_base_addr + (t * 32)`

Decode function contract:

* Input: `(tileIndex, xInTile 0..7, yInTile 0..7)`
* Output: `paletteIndex 0..15` (4-bit)

Document this assumption clearly in code comments and keep the decode isolated so it can be swapped to planar later with minimal blast radius.

---

## 3) Tilemap Format (minimal, deterministic)

### Ground truth constraints

* Visible tilemap region is **32×24 tiles** covering **256×192**.

### Assumption (because entry packing is not explicitly specified)

Use a **16-bit tilemap entry**:

* Bits 0–9: `tileIndex` (0–1023)  *(enough headroom; adjust later if VRAM budget dictates)*
* Bits 10–12: `paletteSelect` (0–7) *(Phase 7: you may force to 0 even if present)*
* Bit 13: HFlip (Phase 7: ignore)
* Bit 14: VFlip (Phase 7: ignore)
* Bit 15: Priority (Phase 7: ignore)

Tilemap address:

* `tilemap_base_addr = PLANE_A_BASE * 1024`
* `entry_addr = tilemap_base_addr + ((tileY * 32 + tileX) * 2)` (little-endian 16-bit)

Document this assumption clearly and keep tilemap parsing isolated.

---

## 4) Scanline Rendering Integration (mandatory)

### Timing constraints to follow

* **Visible scanlines:** 0–191 → render exactly one scanline each.
* **VBlank scanlines:** 192–261 → do not render.
* VBlank definition is authoritative in the I/O skeleton.

### Register timing rule (Phase 7 simplified)

* **All VDP register writes take effect at the start of the next scanline.**
  Implement a `pending_regs` shadow and “latch” to `active_regs` at scanline start.

### Render algorithm (per scanline Y = 0..191)

At **start of scanline**:

1. `active_regs = pending_regs` (latch)
2. If `active_regs.display_enable == 0`: fill framebuffer line Y with color 0 (black) and return.

Otherwise, render:
3. Compute scrolled coordinates:

* `globalY = (Y + scrollY) mod (24*8)`  *(Phase 7: wrap at 192; later can expand if larger maps exist)*
* `tileY = globalY / 8`, `yInTile = globalY % 8`

4. For each pixel `X = 0..255`:

    * `globalX = (X + scrollX) mod (32*8)`
    * `tileX = globalX / 8`, `xInTile = globalX % 8`
    * Fetch tilemap entry at `(tileX, tileY)` → `tileIndex`, optional attrs
    * Decode tile pixel from pattern VRAM → `pix4bpp`
    * Map `pix4bpp` through palette:

        * Phase 7 minimum: `rgb = fixed_palette[pix4bpp]` (or paletteSelect*16 + pix4bpp if you implement palette banks now)
    * Store RGB into framebuffer at `(X, Y)`.

At **end of frame** (after scanline 261 wraps to 0), present the framebuffer once via the existing SDL texture path.

---

## 5) Interfaces / module boundaries (architecture-compliant)

Do not give the PPU ownership of time. The scheduler drives scanlines; PPU exposes a “render one scanline” entry point.

Implement/update minimal interfaces (names are examples; match your repository conventions):

### `PPU` public surface

* `void reset();`
* `uint8_t io_read(uint8_t port);`
* `void io_write(uint8_t port, uint8_t value);`
* `void begin_scanline(int scanline);`  // latch pending→active, handle per-line state
* `void render_scanline(int scanline, uint32_t* framebuffer_256x192);` // only renders visible scanlines
* `const PpuRegs& debug_regs_active() const;`
* `const PpuRegs& debug_regs_pending() const;`

### Bus routing

Ensure I/O ports `0x10–0x1F` dispatch to PPU (existing Phase 2/3 routing rules unchanged). Unmapped ports still read `0xFF` and ignore writes.

---

## 6) Deterministic Test ROM / Harness (must exist in Phase 7)

Create a minimal ROM (or a host-side harness that executes an equivalent sequence) that proves:

* pattern decode
* tilemap fetch
* scroll application
* display-enable gating
* scanline stability (no mid-frame tearing)

Use the diagnostic ROM spec’s structure as the target workload shape:

### Test content (RAM buffers)

Prepare in Work RAM:

* Palette data buffer (optional for Phase 7, but recommended; Phase 8 will require proper palette timing). The diagnostic spec defines simple entries like black/red/green/blue/white.
* Pattern data for **exactly 4 tiles** (solid, checkerboard, vertical stripes, horizontal stripes).
* Tilemap buffer 32×24 with repeating `[0 1 2 3 0 1 2 3 ...]` rows.

### DMA loads (during VBlank only)

During VBlank:

* DMA pattern bytes → `PATTERN_BASE` region in VRAM.
* DMA tilemap bytes → `PLANE_A_BASE` region in VRAM.
* (Optional) DMA palette bytes → palette RAM (Phase 7 may stub, but keep wiring compatible).

Then:

* Write `VDP_CTRL` to enable display and Plane A (Plane B/sprites disabled).

Optional motion proof:

* Once per frame (main loop or VBlank ISR), increment `scrollX` slowly to show stable scrolling.

---

## 7) Debug visibility (required)

Add/enable ImGui (or your existing debug UI shell) panels:

1. **VRAM viewer (hex)**

* Show address, byte values.
* Quick jump to `PLANE_A_BASE*1024` and `PATTERN_BASE*1024`.

2. **Tile viewer**

* Render a selectable tile index (e.g., 0–255) by decoding pattern VRAM.
* Display as 8×8 enlarged.

3. **Tilemap viewer (optional)**

* Render 32×24 map as a small preview (no scroll) or show selected entry details.

4. **Registers panel**

* Active + pending:

    * Plane A base (0x16)
    * Pattern base (0x18)
    * Scroll X/Y (0x12/0x13)
    * Display enable (0x11)
* Current scanline and vblank_flag

The advisory PDF explicitly calls out VRAM/tilemap viewers and register viewers as key debug tools; implement the minimal subset above.

---

## 8) Phase 7 Pass/Fail Criteria (map to checklist)

Use the Phase 7 checklist as hard acceptance criteria:

**Must pass**

* VRAM pattern data decodes correctly (tile viewer matches expected 4 tiles).
* Tilemap lookup works (repeating 0/1/2/3 pattern visible on screen).
* Plane A scroll registers apply (scrollX changes shift image smoothly).
* Palette indexing correct (even if fixed palette, indices 0–15 map deterministically).
* Scanline renderer produces stable output (no frame-to-frame nondeterminism; no mid-frame application of register writes).

**Fail conditions**

* Any rendering performed during VBlank scanlines.
* Register writes affecting the current scanline (must only apply starting next scanline).
* Image depends on “end of frame VRAM state” rather than per-scanline state.

---

## Deliverables summary

* PPU internal model (active/pending regs, VRAM-backed fetch/decode, framebuffer write)
* I/O register handling for `0x10,0x11,0x12,0x13,0x16,0x18` (scanline-latched)
* Scanline render integration (visible-only) + present once per frame
* Deterministic test ROM/harness that DMA-loads 4 tiles + tilemap and enables display
* Debug panels: VRAM hex + tile viewer + regs + scanline/vblank

Do not implement Plane B/sprites/windowing/palette mid-frame behavior; keep code structured so Phase 8 can add palette timing without rewriting the renderer.
