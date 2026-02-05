You are a coding agent implementing **Super_Z80 Bring-Up Phase 11: Sprites**. Follow the authoritative project docs and **do not** add new timing models, new IRQ sources beyond VBlank (sprite overflow IRQ optional), or audio. Preserve Phase 9 diagnostic ROM compatibility and Phase 10 dual-plane behavior. **Do not write full source code**; produce implementation instructions and **interface skeletons only** (C++-style).

---

## 0) Non-negotiable constraints (from project architecture)

* **PPU owns the sprite engine** and performs **per-scanline sprite evaluation and rendering**.
* **Scheduler owns time**; PPU must not advance time.
* Rendering is **strictly scanline-driven**; apply register changes at **scanline start boundaries**.
* Keep event ordering compatible with the architecture’s per-scanline execution order (CPU runs, then PPU renders visible scanlines, etc.).

---

## 1) Phase 11 goals and pass/fail criteria (map to bring-up checklist)

Implement and validate:

1. **SAT parsed correctly** (48 entries).
2. **Per-scanline selection limit enforced (16)**, with deterministic overflow behavior and a latched overflow flag.
3. **Priority vs planes correct** (sprites composite deterministically with Plane A/B).
4. **No flicker beyond the hardware limit** (i.e., the same first-16 selection every time for a given state; no frame-to-frame rotation unless explicitly added later).

Pass = sprite visuals + overflow behavior match the test ROM expectations; Phase 9 + Phase 10 remain correct.

---

## 2) Sprite registers (I/O ports 0x20–0x2F)

Implement these ports via the Bus → PPU register interface:

### `0x20` SPR_CTRL (R/W)

Minimal subset:

* Bit 0: `SPR_EN` (1 = sprite system enabled; 0 = sprites disabled, SPR_STATUS may still latch overflow if you choose, but recommended: no evaluation when disabled)
* Bits 1–2: `SIZE_MODE` (00=8x8, 01=8x16, 10=16x16, 11=reserved). **Phase 11 may implement 8x8 only**; if non-8x8 selected, either clamp to 8x8 (recommended for bring-up) or treat as “not supported yet” but keep deterministic.

### `0x21` SAT_BASE (R/W)

* Interpreted as a **VRAM page index** selecting the sprite attribute table base.
* **Chosen unit for Phase 11:** 256-byte pages.

    * `sat_base_addr = (SAT_BASE & 0xFF) * 256`
    * SAT consumes 48 entries × 8 bytes = **384 bytes**, spanning 2 pages.
* Behavior if base exceeds VRAM size: wrap using modulo VRAM size (deterministic), and emit a debug warning.

### `0x22` SPR_STATUS (R)

* Bit 0: `OVERFLOW` (latched when >16 sprites intersect a visible scanline; persists until cleared)
* Clear policy (choose one and keep consistent):

    * **Recommended:** clear on **VBlank start** (scanline 192 entry), so games/tests can read a stable overflow result for the last frame.

### `0x23–0x2F` reserved

* Stub reads return 0x00 (or 0xFF if your global unmapped I/O policy does that), but do not add behavior.

---

## 3) Chosen SAT entry format (easy to revise)

Because packing is not locked, implement an 8-byte SAT entry format stored in VRAM at `SAT_BASE`:

**Byte 0:** `Y` (0–255, interpreted as signed wrap if desired; Phase 11 treat as unsigned and allow wrap by modulo 256)
**Byte 1:** `X` (0–255)
**Byte 2:** `TILE_LO`
**Byte 3:** `TILE_HI` (bits 0–3 used; upper bits reserved)
**Byte 4:** `ATTR`

* bits 0–3: `PALETTE_SEL` (0–15)
* bit 4: `PRI_BEHIND_A` (1 = sprite is behind Plane A, but still above Plane B; see compositing)
* bit 5: `FLIP_X`
* bit 6: `FLIP_Y`
* bit 7: reserved
  **Byte 5:** reserved (0)
  **Byte 6:** reserved (0)
  **Byte 7:** reserved (0)

Tile index = `tile = TILE_LO | ((TILE_HI & 0x0F) << 8)`.

**Transparency rule:** color index 0 is transparent for sprites.

---

## 4) Per-scanline evaluation algorithm (mandatory)

For each **visible scanline** `y = 0..191`:

1. If sprites disabled (`SPR_CTRL.SPR_EN==0`): skip evaluation and rendering.
2. Iterate SAT entries **in SAT order 0..47**.
3. For each sprite, determine if it intersects this scanline:

    * Determine sprite height from size mode:

        * Phase 11 minimal: treat all as 8x8 (height=8) unless you implement more.
    * Intersects if `y in [spriteY, spriteY + height - 1]` using 8-bit wrap semantics:

        * Recommended Phase 11 rule: compute `dy = (uint8_t)(y - spriteY)`; intersects if `dy < height`.
        * This naturally supports wrap without special cases.
4. Add qualifying sprites to a `selectedSprites[16]` list until full.
5. If more than 16 qualify:

    * Set overflow latch (`SPR_STATUS.OVERFLOW=1`) **immediately** when the 17th qualifying sprite is found.
    * Do not add further sprites for that scanline.

**Sprite-to-sprite priority rule (chosen):**

* **Earlier SAT index has higher priority.**
* For overlapping sprite pixels, the **earlier index wins** (appears on top).
* Implementation approach: build the sprite line buffer by drawing selected sprites **from last selected → first selected**, so sprite 0 is drawn last (overwriting), making it topmost.

This must be deterministic and match your overflow selection rule.

---

## 5) Sprite pixel rendering (line-based)

Render only the current scanline into a temporary line buffer `spriteLine[256]` of “pixel candidates”:

### Pixel candidate type

Store enough metadata for compositing:

* `bool opaque`
* `uint8_t colorIndex` (1–15; 0 never stored as opaque)
* `uint8_t paletteSel` (0–15)
* `bool behindPlaneA` (from ATTR)
* `uint8_t spriteIndex` (0–47) for debug (optional)

### Scanline render steps

For each selected sprite:

1. Compute `dy` within sprite (0..height-1), apply `FLIP_Y` to choose source row.
2. For each `x` pixel across sprite width (Phase 11 minimal: 8):

    * screenX = spriteX + sx (uint8 wrap is fine)
    * Apply `FLIP_X` to choose source column.
3. Decode the sprite tile pixel using **the same pattern format as background tiles** (already working by Phase 7/10). Do not invent a new decode path; reuse/refactor the existing tile decode helper.
4. If decoded pixel index == 0: transparent → do nothing.
5. Else write into `spriteLine[screenX]` if either:

    * target is empty, or
    * current sprite has higher priority than what’s already there (per the rule: earlier SAT index wins; if you render in reverse order, you can simply “overwrite unconditionally” because higher priority sprites are drawn last).

---

## 6) Compositing with Plane B / Plane A (deterministic)

You already have Phase 10 compositing rules for Plane B and Plane A. **Do not change those.** Extend the mixer by inserting sprite pixels in a deterministic way.

### Base layer order (required minimum)

* Base background stack (existing): **Plane B → Plane A** (with whatever priority rules you implemented in Phase 10)
* Then apply sprites with two modes:

    1. **Normal sprite priority**: sprite appears **above both planes**.
    2. **Behind flag set (`PRI_BEHIND_A=1`)**: sprite appears **between Plane B and Plane A**.

### Minimal deterministic mixing rule

For each pixel `x` in the scanline:

1. Compute `pixB` from Plane B (existing).
2. Compute `pixA` from Plane A (existing).
3. Compute `pixS` from spriteLine (if any).
4. Produce final output:

    * If no sprite pixel → output existing background composite result.
    * If sprite pixel exists:

        * If `pixS.behindPlaneA == 0`: sprite overlays final background (above both planes).
        * If `pixS.behindPlaneA == 1`: sprite overlays Plane B **but is occluded by Plane A opaque pixels**.

            * Meaning: if Plane A pixel is opaque (non-zero after its own transparency rules), Plane A wins; else sprite wins over Plane B.

### Palette snapshot rule

Use the same “palette visibility boundary” rule you established in Phase 8 (mid-frame updates apply at next scanline boundary). Sprites must use the **active palette snapshot for the scanline** (same as planes).

---

## 7) Optional: sprite overflow IRQ (only if easy)

You may implement a sprite overflow IRQ as an optional extension, but **do not introduce new IRQ sources beyond VBlank unless it is contained and masked/acknowledged via existing IRQ infrastructure**.

If implemented:

* When overflow occurs during a visible scanline, set a pending bit `SPR_OVERFLOW` in IRQ status.
* Respect IRQ_ENABLE mask.
* Allow clearing via IRQ_ACK.
  If not implemented:
* Only set `SPR_STATUS.OVERFLOW` latch, and expose it in debug UI.

---

## 8) Interfaces and module boundaries (skeletons only)

Add/extend these PPU-facing interfaces. Keep data access read-only for debug.

### PPU public surface (example skeleton)

```cpp
struct SpriteEntry {
  uint8_t y;
  uint8_t x;
  uint16_t tile;
  uint8_t palette;
  bool behindPlaneA;
  bool flipX;
  bool flipY;
};

struct SpriteScanlineSelection {
  uint16_t scanline;                 // 0..191
  uint8_t count;                     // 0..16
  uint8_t indices[16];               // SAT indices selected in evaluation order
  bool overflowThisLine;             // true if >16 found
};

struct SpriteDebugState {
  bool enabled;
  uint8_t spr_ctrl;
  uint8_t sat_base;
  bool overflowLatched;
  SpriteEntry sprites[48];           // decoded view of SAT as last read (or live read helper)
  SpriteScanlineSelection lastSel;   // last evaluated visible scanline (or per-scanline ring buffer if desired)
  bool showSpritesOnly;              // debug toggle
  bool showComposite;                // debug toggle
};

class PPU {
public:
  // I/O ports
  uint8_t ReadPort(uint8_t port);
  void    WritePort(uint8_t port, uint8_t value);

  // Called by scheduler for visible scanlines only (architecture order must be preserved)
  void RenderScanline(uint16_t scanline, uint32_t* outRGBA256);

  // Debug
  const SpriteDebugState& GetSpriteDebugState() const;
};
```

### Internal helpers to require

* `DecodeSATEntry(int i) -> SpriteEntry` (reads VRAM at sat_base_addr + i*8)
* `EvaluateSpritesForScanline(scanline) -> SpriteScanlineSelection`
* `RenderSpriteLine(scanline, selection, outSpriteLine[256])`
* `CompositeLine(scanline, planeBLine, planeALine, spriteLine, outRGBA256)`

Keep these strictly within PPU; CPU must not call them directly.

---

## 9) Deterministic test ROM / harness (Phase 11)

Create a small deterministic sprite test ROM that:

1. Initializes video, enables both planes (Phase 10 behavior), loads:

    * A simple tile set for planes (existing path)
    * A small set of sprite tiles in VRAM pattern region (same tile format)
2. Writes SAT entries to VRAM at SAT_BASE:

    * Place sprites over both planes in known positions:

        * Some sprites fully opaque
        * Some sprites with transparency holes (palette index 0) to validate transparency
        * At least one sprite with `behindPlaneA=1` that passes behind Plane A but in front of Plane B
3. Moves one sprite horizontally each frame (e.g., X++), keeping everything else constant.
4. Creates a scanline (pick a visible y like 80) with **>16 sprites intersecting**:

    * e.g., 20 sprites all with y=80 and different x positions.
    * Verify:

        * Only the first 16 in SAT order appear
        * Overflow latch sets
        * Remaining 4 are absent on that scanline
5. Expected output:

    * Stable sprites, no nondeterministic flicker
    * Correct behind/above behavior
    * Overflow latch visible via SPR_STATUS and debug

Add a “golden checksum” option for automated validation:

* During VBlank, compute a simple checksum of the framebuffer (or selected scanlines) and compare to an expected value written to RAM for the harness to read.

---

## 10) Debug visibility requirements (Phase 11)

Add an ImGui (or existing debug UI) panel with:

1. **Sprite list inspector (48 entries)** showing decoded fields (x,y,tile,palette,behind,flipX,flipY).
2. **Per-scanline selection view**:

    * Current scanline (or last rendered scanline)
    * Selected sprite indices (0..15)
    * Overflow indicator and the first 16 chosen
3. Visual toggles:

    * “Sprites only” (render only spriteLine)
    * “Composite” (normal output)

Debug inspection must be side-effect free.

---

## 11) Implementation order (recommended)

1. Implement ports 0x20–0x22 and internal latches (SPR_CTRL/SAT_BASE/SPR_STATUS).
2. Implement SAT decode for a single entry; add debug list view for all 48.
3. Implement scanline evaluation and overflow latch.
4. Implement sprite line renderer (8x8 only) reusing existing tile decode.
5. Integrate compositing using the “behind Plane A” rule without changing existing plane logic.
6. Build and run the sprite test ROM; verify deterministic results.
7. Re-run Phase 9 diagnostic ROM and Phase 10 parallax checks to ensure no regressions.

---

## 12) Phase 11 “done” checklist (must be demonstrably true)

* SAT parsing correct and visible in debug.
* 16-per-scanline rule enforced; overflow latch sets reliably and clears at the defined boundary.
* Sprites render with transparency (palette index 0 transparent).
* Deterministic sprite priority matches the chosen rule (earlier SAT index wins).
* Compositing obeys:

    * normal sprites above both planes
    * behind sprites between B and A (occluded by A)
* Phase 9 and Phase 10 remain passing.

Produce only the described skeletons and implementation steps; do not generate full code.
