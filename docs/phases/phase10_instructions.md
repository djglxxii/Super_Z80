## CODING-AGENT PROMPT — Super_Z80 Bring-Up Phase 10: Plane B (Dual Background / Parallax)

### Scope / hard constraints (do not violate)

* Implement **Plane B** as a second tilemap layer with **independent base + scroll**.
* **No sprites** (Phase 11). No window/HUD split. No new IRQ sources beyond VBlank.
* Preserve **Phase 9 diagnostic ROM compatibility**: Plane A path must remain correct; Plane B must not interfere when disabled. 【turn12file6†super_z80_emulator_bringup_checklist.md†L44-L74】【turn12file11†super_z80_diagnostic_cartridge_rom_spec.md†L19-L29】

### Authoritative architectural/timing rules you must follow

* PPU renders **strictly in scanline order** and **applies register changes at defined boundaries (scanline start)**. 【turn12file2†super_z80_emulator_architecture.md†L21-L25】
* Scheduling is scanline-based; visible scanlines are **0–191**, VBlank begins at **scanline 192**. 【turn12file4†super_z80_emulation_timing_model.md†L10-L16】【turn12file4†super_z80_emulation_timing_model.md†L72-L76】
* Video rendering schedule is scanline-based; background planes are rendered each visible scanline, then priority/mix is applied (sprites deferred). 【turn12file8†super_z80_emulation_timing_model.md†L56-L68】

---

## 1) Register semantics to implement (Plane B + enable)

Implement these I/O registers in the Video Control block (`0x10–0x1F`), consistent with the existing Plane A register handling style:

### Plane B fine scroll

* `0x14` — `PLANE_B_SCROLL_X` (R/W)
* `0x15` — `PLANE_B_SCROLL_Y` (R/W)
  Meaning: signed/unsigned is not specified; treat as **wrapping fine scroll** in pixel units (0–255) consistent with Plane A behavior already implemented.

### Plane B base select

* `0x17` — `PLANE_B_BASE` (R/W)
  Meaning: selects the **VRAM base** for Plane B tilemap, in the same units/policy used for `PLANE_A_BASE` (0x16). The I/O map explicitly calls these “VRAM base selectors” and recommends “page indices” (e.g., 1KB pages). Reuse the exact unit conversion logic already used for Plane A base. 【turn12file7†super_z80_io_register_map_skeleton.md†L80-L86】

### Pattern base sharing (Phase 10 decision)

* `0x18` — `PATTERN_BASE` (R/W) is shared by **both** Plane A and Plane B in Phase 10.
  Rationale: Phase 10 requirement states Plane B uses the same pattern base as Plane A unless spec requires separate bases; no separate register exists in the skeleton. 【turn12file7†super_z80_io_register_map_skeleton.md†L80-L86】

### Global plane enable bits

* `0x11` — `VDP_CTRL` (R/W) must allow enabling/disabling Plane B independently (“plane enable masks” are explicitly listed). Implement plane enable flags such that:

    * Plane A enable: existing behavior (do not change).
    * Plane B enable: new bit (choose a bit value and document it in code comments + debug UI; do **not** break existing ROMs). 【turn12file7†super_z80_io_register_map_skeleton.md†L56-L69】

### Boundary-applied writes rule (mandatory)

* **All** writes to `VDP_CTRL`, `PLANE_B_SCROLL_X/Y`, and `PLANE_B_BASE` must be **latched** and become active at **start of the next scanline** (same policy as Phase 8 palette “mid-frame changes affect next scanline”). Implement this by maintaining “latched” vs “active” copies and copying at scanline start. 【turn12file6†super_z80_emulator_bringup_checklist.md†L28-L37】【turn12file2†super_z80_emulator_architecture.md†L23-L25】【turn12file7†super_z80_io_register_map_skeleton.md†L66-L69】

**Interface skeleton (do not fully implement code, only define shape):**

* `struct VdpRegsLatched { ... planeA..., planeB_scroll_x, planeB_scroll_y, planeB_base, vdp_ctrl ... }`
* `struct VdpRegsActive  { ... same fields ... }`
* `PPU::OnScanlineStart(int scanline)` does `active = latched` (or field-by-field if needed).

---

## 2) Tilemap / pattern usage (Plane B mirrors Plane A in Phase 10)

Hardware spec states both planes share:

* Tile size 8×8, 4bpp (16 colors per tile)
* Tilemap size **32×24 tiles**
* Independent fine scroll per plane
* Attributes include palette select, priority, flip. 【turn12file12†super_z280_hardware_specification.md†L11-L20】

Phase 10 implementation requirement:

* Tilemap dimensions and entry format: **identical** to Plane A *as currently implemented*.
* Pattern decode path: **identical** to Plane A; both planes reference `PATTERN_BASE` (0x18). 【turn12file7†super_z80_io_register_map_skeleton.md†L80-L86】

**Actionable instruction:**

* Extract your existing Plane A per-scanline tile fetch function into a reusable helper:

    * `RenderTilePlaneScanline(PlaneId plane, const VdpRegsActive& regs, uint8_t outLine[256])`
* Parameterize by:

    * tilemap base (A: 0x16, B: 0x17)
    * scroll x/y (A: 0x12/0x13, B: 0x14/0x15)
    * shared pattern base (0x18)
    * plane enable (from `VDP_CTRL`)

---

## 3) Compositing / priority rule (Phase 10 deterministic minimum)

### Phase 10 baseline rule (must implement)

* Default plane ordering: **Plane B is behind Plane A** (parallax background).
  Meaning: when both planes produce a non-transparent pixel at the same screen coordinate, **Plane A wins**.

### Transparency rule (required for compositing)

* Treat palette index **0** as transparent for tile pixels during plane-plane compositing (consistent with typical tile systems; do not change palette lookup mechanics otherwise). Keep Phase 8 palette lookup behavior unchanged. 【turn12file6†super_z80_emulator_bringup_checklist.md†L28-L40】

### Optional per-tile priority (only if already unambiguous in your implementation)

Hardware spec says tiles have a priority attribute. 【turn12file12†super_z280_hardware_specification.md†L15-L20】

However, the I/O register map skeleton does **not** lock the tile entry bit layout in Phase 10. Therefore:

* **Primary implementation choice for Phase 10:** implement **global plane ordering only** (B behind A), and defer per-tile priority until the tile attribute format is explicitly locked for sprites/advanced phases.
* **Compatibility clause:** if your current Plane A tilemap decoder already extracts a “priority bit” (from earlier phases/tests), you may optionally support:

    * If A tile has priority=1 and B pixel is nonzero: A wins (already true under global rule).
    * If B tile has priority=1 and A pixel is nonzero: B wins (overrides global rule).
    * If both set: keep Plane A winning (deterministic tie-break).

**Compositor skeleton:**

* Inputs: `lineB[256]`, `lineA[256]`
* Output: `finalLine[256]` (palette indices or final RGB, depending on your Phase 8 pipeline)
* Logic:

    1. Start with `final = lineB`
    2. For each x: if `lineA[x] != 0` then `final[x] = lineA[x]`
    3. (Optional) apply per-tile priority override only if decoder provides it without guessing.

---

## 4) Scanline renderer changes (required algorithm)

For each visible scanline (0–191):

1. At scanline start: copy latched regs → active regs (boundary rule).
2. If display disabled: output blank line (existing behavior).
3. If Plane B enabled: render Plane B scanline into `lineB` (palette indices).

    * Else fill `lineB` with 0.
4. If Plane A enabled: render Plane A scanline into `lineA`.

    * Else fill `lineA` with 0.
5. Composite `lineA` + `lineB` into `lineOut` using Phase 10 priority rule.
6. Convert `lineOut` via palette lookup (exactly as in Phase 8) into the final framebuffer row (or keep as indices if Phase 8 already does late palette resolve—do not alter that architecture).

**Important:** The timing model’s ordering text lists “Render Plane A background, Render Plane B background, … Apply priority and palette” — you are free to render into temporary buffers in any order as long as the final output matches the deterministic rule and scanline boundary rules. 【turn12file8†super_z80_emulation_timing_model.md†L60-L68】

**Data structures to add (skeleton only):**

* `std::array<uint8_t, 256> m_linePlaneA;`
* `std::array<uint8_t, 256> m_linePlaneB;`
* `std::array<uint8_t, 256> m_lineComposite;` (or reuse one buffer)

---

## 5) Test ROM / harness (required)

Create a dedicated **Phase 10 Plane B** test ROM (recommended) rather than modifying the Phase 9 diagnostic ROM, to avoid destabilizing its pass criteria.

### Test goals

* Demonstrate Plane B uses **independent base** + **independent scroll**.
* Demonstrate deterministic compositing: B behind A (A occludes B where both draw).
* Demonstrate no corruption over time.

### Minimum ROM behavior

1. Initialize VDP:

    * Enable display
    * Enable Plane A
    * Enable Plane B
    * Sprites disabled (keep). (Diagnostic ROM shows how to enable A and disable B; you will invert that for this ROM.) 【turn12file11†super_z80_diagnostic_cartridge_rom_spec.md†L19-L29】
2. Load distinct patterns:

    * Tile set for Plane A: obvious “foreground” pattern (e.g., solid blocks / bold stripes).
    * Tile set for Plane B: different obvious pattern (e.g., checkerboard).
    * Use DMA during VBlank per established model (reuse your existing DMA harness; do not introduce mid-frame VRAM writes). 【turn12file4†super_z80_emulation_timing_model.md†L106-L116】【turn12file11†super_z80_diagnostic_cartridge_rom_spec.md†L76-L83】
3. Create two tilemaps in RAM:

    * Plane A tilemap: e.g., big text or repeated bars with palette indices that contrast with B.
    * Plane B tilemap: repeating pattern across the full 32×24.
4. DMA both tilemaps into VRAM at **different base pages**:

    * Write `PLANE_A_BASE` and `PLANE_B_BASE` to different values.
5. Main loop:

    * Plane A scroll fixed (0,0)
    * Plane B scroll changes slowly (e.g., `scroll_x++` each frame, or every N frames) to simulate parallax.
    * Optionally vary Y scroll at a different rate to ensure both axes work.

### Expected output

* Two stable layers with distinct visuals.
* Plane B moves smoothly; Plane A remains static (or moves differently if you choose).
* Where Plane A draws nonzero pixels, Plane B is hidden (B behind A).
* No tearing/corruption; behavior deterministic across runs.

---

## 6) Debug visibility (required)

Add or extend debug views so Plane B state is inspectable without side effects (architecture requirement). 【turn12file0†super_z80_emulator_architecture.md†L28-L31】【turn12file2†super_z80_emulator_architecture.md†L147-L163】

### Required debug elements

* Register viewer:

    * Show `PLANE_B_SCROLL_X`, `PLANE_B_SCROLL_Y`, `PLANE_B_BASE`
    * Show `VDP_CTRL` plane enable bits, including Plane B enable
    * Prefer showing both **latched** and **active** values to make boundary behavior visible.
* Tilemap viewer:

    * Add a toggle A/B to view the selected plane’s tilemap at its base.
* Output toggles (optional but recommended):

    * Show only Plane A
    * Show only Plane B
    * Show composited output

(These debug expectations align with the project’s emphasis on register/tilemap visibility.) 【turn12file3†Designing a Cycle-Accurate Emulator for the Super_Z80 Console.pdf†L12-L16】

---

## 7) Phase 10 acceptance criteria (map to bring-up checklist)

Implement and verify each checklist item from Phase 10:

1. **Plane B base registers respected**

    * Changing `PLANE_B_BASE` selects a different tilemap region in VRAM without affecting Plane A. 【turn12file6†super_z80_emulator_bringup_checklist.md†L65-L71】
2. **Independent scroll works**

    * Plane B scroll X/Y changes move only Plane B; Plane A remains unaffected. 【turn12file6†super_z80_emulator_bringup_checklist.md†L65-L71】【turn12file12†super_z280_hardware_specification.md†L13-L15】
3. **Priority rules honored**

    * Default B behind A compositing is correct and stable (and optional per-tile override only if non-ambiguous). 【turn12file6†super_z80_emulator_bringup_checklist.md†L65-L71】
4. **No interference with Plane A**

    * With Plane B disabled in `VDP_CTRL`, output is identical to Phase 9 behavior; diagnostic ROM still passes. 【turn12file6†super_z80_emulator_bringup_checklist.md†L69-L74】【turn12file11†super_z80_diagnostic_cartridge_rom_spec.md†L19-L29】

**Pass criteria:** Dual-plane rendering behaves as expected (stable parallax, deterministic compositing, no regressions). 【turn12file6†super_z80_emulator_bringup_checklist.md†L72-L74】

---

### Deliverable summary (what you must produce in codebase; no full code in this response)

* Plane B register plumbing (`0x14`, `0x15`, `0x17`) + Plane B enable in `VDP_CTRL` (`0x11`) with boundary-applied semantics. 【turn12file7†super_z80_io_register_map_skeleton.md†L56-L86】
* PPU scanline renderer updated to render Plane B into a line buffer, Plane A into a line buffer, composite deterministically, then palette-resolve exactly as in Phase 8. 【turn12file8†super_z80_emulation_timing_model.md†L60-L68】
* A dedicated Phase 10 parallax test ROM/harness and a short test plan.
* Debug UI enhancements: Plane B regs + A/B tilemap viewer + optional layer toggles.
