#ifndef SUPERZ80_DEVICES_APU_APU_H
#define SUPERZ80_DEVICES_APU_APU_H

#include <array>
#include <cstdint>
#include <memory>

#include "devices/apu/AudioRingBuffer.h"
#include "devices/apu/PCM2Ch.h"
#include "devices/apu/SN76489_PSG.h"
#include "devices/apu/YM2151_OPM.h"

namespace sz::apu {

// Canonical clock constants (single source of truth)
constexpr double kAPU_CPU_HZ = 5369317.5;
constexpr double kAPU_PSG_HZ = 3579545.0;
constexpr double kAPU_OPM_HZ = 3579545.0;
constexpr int kAPU_SAMPLE_RATE = 48000;
constexpr int kAPU_RING_CAPACITY_FRAMES = 16384;

struct APUConfig {
  double cpu_hz = kAPU_CPU_HZ;
  double psg_hz = kAPU_PSG_HZ;
  double opm_hz = kAPU_OPM_HZ;
  int sample_rate = kAPU_SAMPLE_RATE;
  int ring_capacity_frames = kAPU_RING_CAPACITY_FRAMES;
};

struct APUAudioStats {
  uint64_t total_frames_generated = 0;
  uint64_t underruns = 0;
  uint64_t overflows = 0;
  int ring_fill_frames = 0;
  int ring_capacity_frames = 0;
  double cpu_hz = 0.0;
  double psg_hz = 0.0;
  double opm_hz = 0.0;
  int sample_rate = 0;
};

struct APUDebugLastWrite {
  uint64_t cpu_cycle_timestamp = 0;
  uint16_t port = 0;
  uint8_t value = 0;
};

// Phase 12: Full APU debug state (replaces stub)
struct DebugState {
  APUAudioStats stats;
  bool psg_muted = false;
  bool opm_muted = false;
  bool pcm_muted = false;
};

class APU {
 public:
  APU();
  ~APU();

  void Reset();

  void AttachCartridgeROM(const uint8_t* rom_data, size_t rom_size);

  // I/O dispatch (called by Bus for ports 0x60-0x7D)
  void IO_Write(uint8_t port, uint8_t value, uint64_t cpu_cycle);
  uint8_t IO_Read(uint8_t port);

  // Called by Scheduler with elapsed CPU cycles for this scanline slice
  void Advance(uint32_t cpu_cycles_elapsed);

  // SDL audio callback pulls interleaved int16 stereo frames.
  // Returns number of frames actually read.
  int PopAudioFrames(int16_t* out_interleaved_lr, int frames);

  // Debug
  DebugState GetDebugState() const;
  APUAudioStats GetStats() const;

  static constexpr int kMaxLastWrites = 64;
  void GetLastWrites(APUDebugLastWrite* out, int max, int& out_count) const;

  void SetMutePSG(bool mute);
  void SetMuteOPM(bool mute);
  void SetMutePCM(bool mute);

  bool IsPSGMuted() const { return psg_muted_; }
  bool IsOPMMuted() const { return opm_muted_; }
  bool IsPCMMuted() const { return pcm_muted_; }

 private:
  void GenerateFrames(int frames);

  APUConfig config_;

  // Chip instances
  SN76489_PSG psg_;
  YM2151_OPM opm_;
  PCM2Ch pcm_;

  // Ring buffer
  std::unique_ptr<AudioRingBuffer> ring_;

  // Cycle-to-sample fixed-point accumulator (Q32.32 equivalent using uint64)
  uint64_t cpu_cycle_accum_fp_ = 0;
  uint64_t cycles_per_sample_fp_ = 0;  // Q32.32

  // Mixer gains
  float psg_gain_ = 0.20f;
  float opm_gain_ = 0.35f;
  float pcm_gain_ = 0.35f;

  // Master volume (0-255, register at 0x7C)
  uint8_t master_vol_ = 0xFF;
  uint8_t audio_pan_ = 0xFF;  // 0x7D, optional

  // Mute toggles
  bool psg_muted_ = false;
  bool opm_muted_ = false;
  bool pcm_muted_ = false;

  // Debug: last writes ring buffer
  std::array<APUDebugLastWrite, kMaxLastWrites> last_writes_ = {};
  int last_write_head_ = 0;
  int last_write_count_ = 0;

  // Stats
  uint64_t total_frames_generated_ = 0;
  uint64_t underrun_count_ = 0;
  uint64_t overflow_count_ = 0;

  // CPU cycle counter (for timestamping writes)
  uint64_t cpu_cycle_counter_ = 0;

  void RecordWrite(uint16_t port, uint8_t value);
};

}  // namespace sz::apu

#endif
