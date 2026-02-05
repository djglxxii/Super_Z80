You are a coding agent implementing **Super_Z80 Bring-Up Phase 12: Audio Bring-Up (PSG + YM2151 + PCM)**.

Authoritative constraints:

* **Do not** change CPU core, IRQ behavior, DMA behavior, or PPU rendering behavior except what is strictly needed to integrate audio.
* **Do not** step audio “per frame.” Audio must advance continuously based on the Scheduler’s time progression (scanlines/cycles).
* If you observe long-run drift, **fix timing math / scheduling**, not video pacing.

You must implement:

* **PSG**: SN76489-style single write port at `0x60`.
* **YM2151 (OPM)**: address port `0x70`, data port `0x71` (reads optional; return `0xFF` if unsupported).
* **PCM (2 channels)**: trigger-based one-shot playback using I/O ports `0x72–0x7B` and master controls `0x7C–0x7D`.
* Mix PSG + OPM + PCM to a **single stereo stream**.
* Output via **SDL2 audio** without blocking the emulator loop (ring buffer + callback).

Hardware targets (locked by spec):

* Audio components and counts: PSG (SN76489-style), YM2151 OPM, PCM with **2 channels**, 8-bit samples, trigger-based one-shot; samples sourced from **cartridge ROM**.
* PCM channels are defined as register groups (START, LEN, VOL, CTRL) with `CTRL` including TRIGGER and BUSY semantics (BUSY reflected on read).

Core choices (license-irrelevant for this task):

* PSG: use a known-good SN76489 implementation (MAME-derived acceptable).
* YM2151: **Nuked-OPM preferred**, otherwise MAME YM2151.
* PCM: custom minimal “DAC” playback per the port map (unsigned 8-bit).

---

## 1) Integration approach (top-level architecture)

Implement an `APU` module that:

1. Owns chip instances and all APU-visible register latches/state.
2. Exposes I/O handlers called by the Bus for ports `0x60`, `0x70–0x7D`.
3. Exposes `Advance(cpu_cycles_elapsed)` (or `AdvanceTicks(master_ticks)` if Scheduler uses master time).
4. Produces audio samples into a thread-safe ring buffer consumed by SDL2’s audio callback.

Important: keep the emulator core SDL-agnostic except for the “platform” audio backend wrapper (SDL2 device open/callback). Keep the APU/chips pure and deterministic.

---

## 2) Canonical clocks and resampling strategy

Define canonical constants (single source of truth in APU):

* `CPU_HZ = 5_369_317.5` (from timing model / hardware spec; do not round to int in drift-sensitive math).
* `PSG_HZ = 3_579_545.0` (recommended)
* `OPM_HZ = 3_579_545.0` (recommended)
* `AUDIO_SAMPLE_RATE = 48_000` (preferred for SDL stability) OR `44_100` (acceptable). Pick one and keep consistent everywhere.

Time conversion rule:

* Scheduler advances in CPU cycles (or master ticks) and calls `APU::Advance(cycles_elapsed)` at the same cadence it advances other devices (typically once per scanline slice).

Use an accumulator to convert elapsed emulated time into produced audio frames:

* Keep a **64-bit fixed-point** accumulator to avoid floating-point drift.
* Example fixed-point scheme (Q32.32):

    * `sample_frac += cycles_elapsed * AUDIO_SAMPLE_RATE << 32`
    * `samples_to_generate = sample_frac / CPU_HZ` (but CPU_HZ is fractional; handle via rational form below)
    * Better: represent `CPU_HZ` as a rational to avoid floats:

        * If timing model provides exact numerator/denominator for CPU clock, use that.
        * Otherwise, use **double** only to compute a “cycles-per-sample” constant once, then run fixed-point from there.

Recommended robust method (simple and stable):

* Precompute `cycles_per_sample_fp = (CPU_HZ / AUDIO_SAMPLE_RATE) in Q32.32`.
* Maintain `cpu_cycle_fp_accum`:

    * `cpu_cycle_fp_accum += cycles_elapsed << 32`
    * `while cpu_cycle_fp_accum >= cycles_per_sample_fp:`

        * `cpu_cycle_fp_accum -= cycles_per_sample_fp`
        * generate 1 stereo sample frame

This makes audio generation strictly tied to CPU cycle progression and prevents frame-based stepping.

Chip stepping per generated sample:

* You must produce one mixed sample frame at the host sample rate.
* For each sample frame:

    * Step PSG by enough internal ticks to correspond to `PSG_HZ / AUDIO_SAMPLE_RATE` (fractional accumulator per chip).
    * Step OPM by enough internal ticks to correspond to `OPM_HZ / AUDIO_SAMPLE_RATE` (fractional accumulator per chip).
    * Step PCM by advancing its sample pointer based on a “PCM playback rate” (define it explicitly; see PCM section).

If chosen chip cores already provide “render N samples at sample_rate given chip clock,” use their recommended API and pass in:

* chip clock (PSG_HZ / OPM_HZ)
* output sample rate (AUDIO_SAMPLE_RATE)
  and call `render()` for `N` samples each `APU::Advance()` batch.

Either approach is acceptable as long as:

* The number of output samples is derived from elapsed cycles (not frames).
* Rendering is deterministic.

---

## 3) I/O port behavior (must match skeleton)

Implement APU port decode and semantics per I/O skeleton:

### PSG (`0x60–0x6F`)

* `0x60 PSG_DATA (W)`: forward byte to SN76489 write port (latch/data).
* All other PSG ports reserved:

    * reads return `0xFF`, writes ignored.

### YM2151 + PCM (`0x70–0x7F`)

YM2151:

* `0x70 OPM_ADDR (W)`: latch YM2151 register index.
* `0x71 OPM_DATA (W)`: write data to latched index.
* `0x71 OPM_DATA (R optional)`: if your core doesn’t support reads, return `0xFF`.

PCM channel regs (2 channels):

* Channel 0:

    * `0x72 PCM0_START_LO (R/W)`
    * `0x73 PCM0_START_HI (R/W)`
    * `0x74 PCM0_LEN (R/W)`
    * `0x75 PCM0_VOL (R/W)`
    * `0x76 PCM0_CTRL (R/W)` with:

        * bit0 TRIGGER (one-shot)
        * bit1 LOOP (optional; default off)
        * bit7 BUSY (read-only reflection)
* Channel 1:

    * `0x77 PCM1_START_LO (R/W)`
    * `0x78 PCM1_START_HI (R/W)`
    * `0x79 PCM1_LEN (R/W)`
    * `0x7A PCM1_VOL (R/W)`
    * `0x7B PCM1_CTRL (R/W)`

Mixer / master:

* `0x7C AUDIO_MASTER_VOL (R/W)`
* `0x7D AUDIO_PAN (R/W) optional`

Global rule from I/O skeleton:

* Unmapped I/O reads return `0xFF`, writes ignored.

---

## 4) PCM design (custom minimal DAC)

PCM data source:

* Use **cartridge ROM** as the PCM sample address space for Phase 12 (matches hardware spec: “Samples sourced from cartridge ROM”).
* Interpret `START_HI:START_LO` as a **byte address within cartridge ROM** (16-bit address). If ROM is larger than 64KB, define how banking affects PCM reads (recommended for Phase 12: treat START as an absolute address into “currently mapped ROM bank window” OR define it as “absolute ROM offset in bank 0.” Pick one, document it, and make the test ROM match it).

Sample format:

* 8-bit **unsigned** PCM (0–255). Convert to signed centered form for mixing: `s = (byte - 128)`.

Playback rate:

* Phase 12 requires only “trigger-based one-shot playback.” Define an explicit rate:

    * Recommended: **1 PCM byte per output sample** (i.e., PCM plays at AUDIO_SAMPLE_RATE Hz). This is simple and deterministic.
    * That means `LEN` is measured in **bytes/samples**, so a 400-byte sample lasts 400/48000 seconds.
* If you later want a different PCM pitch model, add a `RATE` register in a future phase; do not add now.

Trigger semantics:

* On write to CTRL where bit0 transitions 0→1:

    * latch `cur_addr = START`, `remaining = LEN`, `active = true`, set BUSY=1.
    * clear TRIGGER bit internally after latching (typical “edge-triggered” behavior), but preserve whatever is stored in the visible register if you want. The only hard requirement is one-shot starts reliably.
* Each generated output sample frame:

    * if active:

        * fetch byte from ROM at `cur_addr`, mix scaled by `VOL`
        * `cur_addr++`, `remaining--`
        * if `remaining == 0`: `active=false`, BUSY=0
    * if LOOP enabled and remaining hits 0: restart at START (optional; default off).
* BUSY reflection:

    * When software reads PCM0_CTRL / PCM1_CTRL, bit7 must reflect active/busy state.

Stereo:

* For Phase 12, simplest:

    * Mix PCM equally to L+R.
    * If `AUDIO_PAN` is implemented, allow per-source or per-channel pan later; for now you may treat `AUDIO_PAN` as optional and either stub it or implement global L/R gains.

---

## 5) Mixing model (deterministic, no clipping surprises)

Implement a mixer that:

* Accepts per-source sample values in a common format (recommended internal: `float` in [-1, +1] or `int32`).
* Has per-source enable/mute toggles (debug feature) and per-source gain scalars.
* Applies `AUDIO_MASTER_VOL` at the end.

Recommended mixing pipeline:

1. Get PSG mono sample
2. Get OPM stereo sample (if core outputs stereo; otherwise mono)
3. Get PCM0 mono sample, PCM1 mono sample
4. Apply per-source gains
5. Sum to L/R
6. Apply master volume
7. Soft clip or hard clamp to int16 for SDL output:

    * For Phase 12: hard clamp to int16 is acceptable, but track peak levels in debug so you can tune gains.

Keep defaults conservative to avoid clipping:

* PSG gain ~0.20
* OPM gain ~0.35
* PCM gain depends on VOL; scale VOL to a small range (e.g., `VOL/255 * 0.35`).

---

## 6) SDL2 audio output strategy (non-blocking)

Use **SDL2 audio callback** pulling from a ring buffer:

* Emulator thread produces samples into ring buffer as it advances.
* SDL audio thread callback consumes samples; if underflow, output silence and increment an underrun counter (debug-visible).

Ring buffer requirements:

* Store **interleaved stereo int16** frames (LRLR…).
* Capacity: target ~100–200ms of audio:

    * at 48kHz, 0.1s = 4800 frames; choose capacity e.g. 16384 frames for safety.
* Maintain a “target fill” (e.g., ~50ms) and generate enough samples during `APU::Advance()` to keep near target.
* Do not block emulator loop waiting on audio. If ring buffer is full, drop oldest or stop writing additional frames (but record overflow count).

Thread-safety:

* Use a lock-free single-producer/single-consumer ring buffer if available.
* Otherwise, use a minimal mutex around push/pop in callback (acceptable for Phase 12 if careful), but avoid long-held locks.

SDL configuration:

* Request `AUDIO_S16SYS`, stereo, sample rate = AUDIO_SAMPLE_RATE, samples = 512 or 1024.
* Log actual obtained spec (SDL may adjust). If SDL changes the sample rate, either:

    * Fail fast (preferred for determinism), or
    * Reconfigure APU sample rate to match obtained (but then everything must use obtained rate).

---

## 7) Debug visibility requirements (Phase 12)

Add APU debug state reporting (ImGui or existing debug UI system):

* Last N writes (timestamped in CPU cycles) for:

    * PSG writes to `0x60`
    * OPM_ADDR and OPM_DATA writes
    * PCM regs per channel + master regs
* Ring buffer metrics:

    * capacity frames
    * current fill frames
    * underrun count
    * overflow count
* Clocking / rates:

    * CPU_HZ
    * PSG_HZ / OPM_HZ
    * AUDIO_SAMPLE_RATE
    * computed samples generated per second (rolling average) and expected ratio
* Mute toggles:

    * PSG mute
    * OPM mute
    * PCM mute (and optionally per PCM channel)

---

## 8) Module interfaces (headers only; no full implementations)

Create/modify these headers (names may be adapted to your repo conventions, but keep boundaries clear):

### `apu/APU.h`

```cpp
#pragma once
#include <cstdint>
#include <array>

struct APUConfig {
  double cpu_hz;
  double psg_hz;
  double opm_hz;
  int    sample_rate;     // 44100 or 48000
  int    ring_capacity_frames;
};

struct APUAudioStats {
  uint64_t total_frames_generated = 0;
  uint64_t underruns = 0;
  uint64_t overflows = 0;
  int      ring_fill_frames = 0;
};

struct APUDebugLastWrite {
  uint64_t cpu_cycle_timestamp;
  uint16_t port;
  uint8_t  value;
};

class CartridgeROM; // read-only view used by PCM

class APU {
public:
  explicit APU(const APUConfig& cfg);

  void AttachCartridgeROM(const CartridgeROM* rom);

  // I/O dispatch (called by Bus)
  void IO_Write(uint16_t port, uint8_t value);
  uint8_t IO_Read(uint16_t port);

  // Called by Scheduler with elapsed CPU cycles for this slice
  void Advance(uint32_t cpu_cycles_elapsed);

  // SDL audio callback pulls interleaved int16 stereo frames
  // Returns number of frames actually read (0 => silence fill by caller)
  int PopAudioFrames(int16_t* out_interleaved_lr, int frames);

  // Debug
  APUAudioStats GetStats() const;
  void GetLastWrites(APUDebugLastWrite* out, int max, int& out_count) const;

  void SetMutePSG(bool);
  void SetMuteOPM(bool);
  void SetMutePCM(bool);

private:
  void GenerateFrames(int frames);

  // internal: chip wrappers, pcm engine, mixer, ring buffer, regs
};
```

### `apu/SN76489_PSG.h`

```cpp
#pragma once
#include <cstdint>

class SN76489_PSG {
public:
  void Reset();
  void WriteData(uint8_t v);

  // Either:
  // 1) render N mono frames at sample_rate (chip wrapper does resampling)
  void RenderMono(float* out, int frames);

  // Or:
  // 2) step by chip ticks and return current sample
  float StepOneSample();

  void SetClock(double psg_hz);
  void SetSampleRate(int sample_rate);
};
```

### `apu/YM2151_OPM.h`

```cpp
#pragma once
#include <cstdint>

class YM2151_OPM {
public:
  void Reset();
  void WriteAddress(uint8_t a);
  void WriteData(uint8_t d);
  uint8_t ReadDataOptional(); // return 0xFF if unsupported

  void SetClock(double opm_hz);
  void SetSampleRate(int sample_rate);

  // Preferred: render stereo directly if the core supports it
  void RenderStereo(float* outL, float* outR, int frames);

  void SetMute(bool);
};
```

### `apu/PCM2Ch.h`

```cpp
#pragma once
#include <cstdint>

class CartridgeROM;

struct PCMChannelRegs {
  uint16_t start = 0;
  uint8_t  len = 0;    // Phase 12: 8-bit length per skeleton (0x74/0x79). If you need >255, extend internally but keep port behavior.
  uint8_t  vol = 0;
  uint8_t  ctrl = 0;   // includes TRIGGER/LOOP; BUSY reflected on reads
};

class PCM2Ch {
public:
  void Reset();
  void AttachROM(const CartridgeROM* rom);

  void WriteReg(int ch, int index, uint8_t v); // index: START_LO/HI/LEN/VOL/CTRL
  uint8_t ReadCtrlWithBusy(int ch) const;

  // Render mono or stereo contribution for N frames
  void RenderMono(float* out, int frames);

  void SetSampleRate(int sample_rate);

private:
  // per-channel runtime state: active, cur_addr, remaining, busy, etc.
};
```

### `apu/AudioRingBuffer.h`

```cpp
#pragma once
#include <cstdint>

class AudioRingBuffer {
public:
  explicit AudioRingBuffer(int capacity_frames);

  int CapacityFrames() const;
  int FillFrames() const;

  // Producer: push interleaved int16 stereo
  int Push(const int16_t* interleaved_lr, int frames);

  // Consumer: pop interleaved int16 stereo
  int Pop(int16_t* out_interleaved_lr, int frames);

private:
  // SPSC ring buffer implementation
};
```

### `platform/SDLAudioDevice.h`

```cpp
#pragma once
#include <cstdint>

class APU;

struct SDLAudioSpecOut {
  int sample_rate;
  int channels;
  int samples_per_callback;
};

class SDLAudioDevice {
public:
  bool Open(APU* apu, int requested_sample_rate, SDLAudioSpecOut& out_obtained);
  void Close();
  void Start();
  void Stop();

private:
  static void SDLCALL AudioCallback(void* userdata, uint8_t* stream, int len);
};
```

Bus integration points:

* Ensure Bus routes `OUT (0x60)` etc. to `APU::IO_Write()`, and `IN` to `APU::IO_Read()`.
* Reserved/unmapped reads must return `0xFF` per skeleton.

---

## 9) Deterministic audio test ROM (Phase 12 harness)

Create a minimal ROM that:

* Initializes interrupts as needed (but does not depend on sprites or Plane B changes).
* Writes PSG to play a steady tone, then changes pitch after a delay loop.
* Writes YM2151 to a known simple patch and plays a note.
* Triggers PCM channel 0 to play a short sample stored in ROM.

Port usage must match skeleton:

* PSG: `OUT (0x60),A`
* OPM: `OUT (0x70),addr` then `OUT (0x71),data`
* PCM0: write `START_LO/HI` at `0x72/0x73`, `LEN` at `0x74`, `VOL` at `0x75`, then set TRIGGER in `0x76`

PCM sample data:

* Place an 8-bit unsigned waveform in ROM at a known address (e.g., a short decaying sine-ish table or a click).
* `LEN` should match the byte count.
* Confirm playback stops exactly at LEN and BUSY clears.

Expected audible result:

* PSG tone stable, then a noticeable pitch change.
* YM2151 produces a stable note (even if timbre is basic).
* PCM produces a short sample event; repeatable triggers.

Determinism requirement:

* The ROM’s behavior must be consistent run-to-run (no reliance on host timing).

---

## 10) Phase 12 acceptance criteria (pass/fail)

PASS if all are true:

1. **PSG tone** is audible and pitch-stable over minutes (no gradual detune/drift).
2. **YM2151 output** is audible and stable over minutes.
3. **PCM** triggers reliably; stops exactly at LEN; BUSY reflects active state on reads.
4. No sustained crackle under normal host load:

    * ring buffer stays near target fill
    * underrun count remains near zero during steady-state
5. No long-run A/V drift:

    * video pacing remains correct
    * audio sample generation rate matches expected cycles→samples math (debug-visible)

FAIL if any occur:

* Audio is generated per frame or tied to render loop rather than cycles.
* Audible periodic pops due to buffer underflow/overflow.
* Long-run drift corrected by altering video timing.

---

## 11) Implementation order (recommended)

1. Add APU stub with ring buffer + SDL device open/callback producing silence.
2. Implement cycle-driven `APU::Advance()` sample generation math (still silence) and validate steady buffer fill.
3. Integrate PSG core, verify test ROM PSG tone stable.
4. Integrate YM2151 core, verify test ROM note stable.
5. Implement PCM2Ch using cartridge ROM source and validate trigger/stop/BUSY.
6. Add debug panels: last writes, buffer metrics, mute toggles, rate/clock readouts.
7. Run for 10+ minutes verifying no drift/crackle.

Deliver only headers + integration instructions; do not produce full source files beyond the interface skeletons above.
