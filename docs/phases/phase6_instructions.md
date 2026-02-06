### 0) Hard constraints (do not violate)

* **Do not add PPU rendering logic** beyond the existing Phase 0 test pattern path (VRAM is *not* rendered yet).
* **Do not stall the CPU** for DMA; DMA is instantaneous in emulation.
* **DMA executes only during VBlank** (scanlines **192–261**).
* **No partial DMA**; a DMA either copies the whole length or does nothing.
* Unmapped I/O reads return `0xFF`; writes are ignored.

---

### 1) Module ownership & call order (must match architecture/timing)

Implement as a distinct device owned by `SuperZ80Console`:

* `PPU` owns VRAM storage and exposes **safe** write APIs; **PPU must not perform DMA**.
* `DMAEngine` owns DMA regs/state and enforces legality; **CPU must not trigger DMA directly** (only via Bus I/O writes).
* `Bus` decodes I/O ports `0x30–0x36` and forwards reads/writes to `DMAEngine`.

**Per-scanline execution order is mandatory** (do not reorder):

1. Scheduler computes CPU cycles for the scanline
2. CPU executes those cycles
3. IRQController updates `/INT` line
4. If visible scanline: PPU renders scanline (existing Phase 0 path only)
5. If scanline == 192: enter VBlank + raise VBlank IRQ
6. **DMAEngine processes queued DMA (if any)**
7. APU advances audio time (exists but no new audio work)
8. Advance scanline

**Important timing guarantee:** VBlank IRQ asserts at **start of scanline 192**.

---

### 2) VRAM backing store (PPU-owned)

Add a VRAM byte array to the PPU:

* If `super_z80_hardware_specification.md` does not explicitly lock VRAM size in your repository copy, define:

    * `static constexpr size_t kVramSizeBytes = /* placeholder */;`
    * Make it *one-line* to revise later.
* Provide PPU APIs that let DMA write without exposing raw pointers publicly:

    * `void PPU::VramWriteByte(uint16_t addr, uint8_t value);`
    * `uint8_t PPU::VramReadByte(uint16_t addr) const;` (debug/tests)
    * `void PPU::VramWriteBlock(uint16_t dst, span<const uint8_t> bytes);` (optional helper)
* VRAM addressing policy (choose + document in code comments):

    * **Recommended**: **wrap** destination address modulo `kVramSizeBytes` for deterministic behavior.
    * Implement as: `effective = addr % kVramSizeBytes`.

---

### 3) DMA I/O registers (ports `0x30–0x36`) and default behavior

Implement DMA register ports in `DMAEngine` and route via Bus I/O decode:

**Registers (R/W unless noted):**

* `0x30` `DMA_SRC_LO`
* `0x31` `DMA_SRC_HI`
* `0x32` `DMA_DST_LO` (VRAM address)
* `0x33` `DMA_DST_HI`
* `0x34` `DMA_LEN_LO`
* `0x35` `DMA_LEN_HI`
* `0x36` `DMA_CTRL` (R/W): START + BUSY reflection + QUEUE_IF_NOT_VBLANK bit

All other ports in `0x30–0x3F` not listed above:

* reads return `0xFF`
* writes ignored

**DMA_CTRL bits (define explicitly in code, stable within emulator):**

* `bit0 = START` (write-1 triggers DMA attempt; reading START can return 0)
* `bit1 = QUEUE_IF_NOT_VBLANK` (policy bit)
* `bit7 = BUSY` (read-only reflection; always 0 except during the exact call if you transiently set it—since DMA is instantaneous, it should read as 0 in practice)

**Policy decisions (lock these for Phase 6):**

* **Length rule (16-bit)**: `len = (DMA_LEN_HI<<8) | DMA_LEN_LO`

    * **Choose**: `len == 0 => 0` (no-op) for simplicity unless the hardware spec explicitly requires `0 => 0x10000`.
* **Source addressing**: `src` is CPU address space; DMA must read bytes via **Bus memory read** so ROM/RAM mapping rules apply. (Do not read RAM arrays directly.)
* **Destination addressing**: `dst` is VRAM address; write via PPU API; apply wrap policy.

---

### 4) DMA legality & execution rules (VBlank-only + queue option)

The emulator must enforce:

* DMA legal only during VBlank scanlines **192–261**.
* If triggered during VBlank: execute instantly; CPU is not stalled.
* If triggered outside VBlank:

    * If `QUEUE_IF_NOT_VBLANK=1`: store as a **queued request**; execute it **exactly at the start of next VBlank** (when scanline becomes 192, in the scanline loop step “DMAEngine processes queued DMA”).
    * Else: ignore the request; optionally set a **debug-only internal error flag** (not CPU-visible yet).

Add an internal invariant:

* `assert(vblank_flag == true)` inside the function that actually performs the copy (or guard + debug assert). The I/O skeleton explicitly guarantees “DMA legal only during VBlank; policy defined by queue bit.”

**Queued DMA semantics (define precisely):**

* Queue captures a snapshot of `{src, dst, len}` at the moment START is written.
* If START is written again while a request is already queued:

    * Pick one policy and document it:

        * simplest: “last write wins” (overwrite queued request)
        * or “ignore new START while queued”
    * Ensure deterministic behavior either way (no accumulation unless you intentionally implement a FIFO).

---

### 5) Interfaces and state structs (skeletons only; no full implementation)

Add minimal “contracts” to make wiring clear and debuggable:

**DMAEngine public surface:**

* `uint8_t ReadReg(uint8_t port);`
* `void WriteReg(uint8_t port, uint8_t value);`
* `void OnScanlineBoundary(int scanline, bool vblank_flag);` *(or `TickScanline(...)`)*

    * Called once per scanline from the console/scheduler loop, in the mandated order.

**DMAEngine dependencies (constructor injection):**

* `Bus& bus` (for memory reads)
* `PPU& ppu` (for VRAM writes)
* `IRQController* irq` (optional DMA_DONE integration; keep optional)

**Debug state exposure (read-only snapshot):**

* `struct DmaDebugState { uint16_t src, dst, len; bool queue_enabled; bool queued_valid; uint16_t queued_src, queued_dst, queued_len; int last_exec_frame, last_exec_scanline; bool last_trigger_was_queued; bool last_illegal_start; };`
* Expose via `DmaDebugState DMAEngine::GetDebugState() const;`

**PPU VRAM debug support:**

* Provide a function to dump a small window (for UI/log): `std::vector<uint8_t> PPU::VramReadWindow(uint16_t start, size_t count) const;`

---

### 6) Optional DMA_DONE IRQ (allowed but not required for Phase 6 pass)

If implemented:

* Use `IRQ_STATUS bit 4 = DMA_DONE pending (optional)` and integrate with `IRQ_ENABLE` + `IRQ_ACK` behavior.
* Pending bits are latched and cleared only by `IRQ_ACK` (write-1-to-clear).
  If you skip DMA_DONE IRQ, Phase 6 can still pass.

---

### 7) Test ROM behavior (describe + implement minimal assembly/C harness; do not overbuild)

Create a Phase 6 test ROM that validates:

1. **Positive DMA during VBlank**

    * Fill a RAM buffer (e.g., 256 bytes) with pattern `p`.
    * During VBlank, program DMA regs and START:

        * SRC = RAM buffer address
        * DST = fixed VRAM address (e.g., `0x2000`)
        * LEN = 256
    * Each frame increment `p` (or XOR with frame counter) so corruption is visible.

2. **Negative test: mid-frame DMA with QUEUE disabled**

    * During visible scanlines, write START with `QUEUE_IF_NOT_VBLANK=0`.
    * Verify VRAM does **not** change as a result.

3. **Queue test: mid-frame DMA with QUEUE enabled**

    * During visible scanlines, write START with `QUEUE_IF_NOT_VBLANK=1`.
    * Verify VRAM changes **exactly at next VBlank start** (scanline 192).

**How the ROM “verifies VRAM” without CPU VRAM reads:**

* Since VRAM is PPU-owned and not necessarily CPU-readable yet, verification can be via:

    * a debug-exposed “VRAM compare port” (not recommended yet), OR
    * **emulator-side debug checks** (recommended for Phase 6): at known frame/scanline, read PPU VRAM and compare to expected pattern.

---

### 8) Emulator-side validation & debug visibility (required)

Add a debug panel or structured log with:

* Current DMA registers (`src/dst/len/ctrl bits`)
* Queued DMA state (valid? snapshot values)
* Last DMA execution `{frame, scanline}` and whether it was queued
* VRAM hex dump window: start address + N bytes (e.g., 64 bytes)

Add explicit assertions / checks:

* Assert that the “perform copy” routine runs only when `vblank_flag==true`.
* Add a deterministic check at end of VBlank (or at start of visible scanline 0 next frame): compare VRAM window against expected buffer, and fail loudly if mismatch.

---

### 9) Phase 6 pass/fail criteria (map to checklist)

Your implementation is considered Phase 6 “done” only if all are true:

* DMA executes **only when VBlank is true**.
* Queued DMA completes **exactly at next VBlank start** (scanline 192), not earlier/later.
* VRAM contents **exactly match source RAM** after VBlank.
* Mid-frame DMA with queue disabled **does nothing** (fail safely).
* **No CPU stalls**; scheduler counters remain stable (DMA is instantaneous; CPU already ran for the scanline).
* No rendering changes (still test pattern).

---

### 10) Implementation notes (avoid common correctness traps)

* START is edge-like: treat “write with START=1” as the trigger; do not require START to be cleared by software.
* When executing DMA, use `for i in [0..len)`:

    * `byte = bus.Read8(src+i)`
    * `ppu.VramWriteByte(dst+i, byte)` (with wrap)
* Ensure determinism: all branching decisions depend only on current scanline/vblank flag and register state, not wall-clock timing.

---

If you need to choose between “queue vs ignore” for illegal DMA triggers: both are permitted by the timing model, but **queue is recommended**.
