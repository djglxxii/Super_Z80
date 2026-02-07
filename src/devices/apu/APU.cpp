#include "devices/apu/APU.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/log/Logger.h"

namespace sz::apu {

APU::APU() {
  config_ = {};
  ring_ = std::make_unique<AudioRingBuffer>(config_.ring_capacity_frames);

  // Precompute Q32.32 cycles_per_sample: (CPU_HZ / SAMPLE_RATE) << 32
  double cps = config_.cpu_hz / config_.sample_rate;
  cycles_per_sample_fp_ = static_cast<uint64_t>(cps * 4294967296.0);  // * 2^32

  // Init chips
  psg_.SetClock(config_.psg_hz);
  psg_.SetSampleRate(config_.sample_rate);
  psg_.Reset();

  opm_.SetClock(config_.opm_hz);
  opm_.SetSampleRate(config_.sample_rate);
  opm_.Reset();

  SZ_LOG_INFO("APU: CPU_HZ=%.1f PSG_HZ=%.1f OPM_HZ=%.1f SampleRate=%d RingCap=%d",
              config_.cpu_hz, config_.psg_hz, config_.opm_hz,
              config_.sample_rate, config_.ring_capacity_frames);
  SZ_LOG_INFO("APU: cycles_per_sample=%.6f (Q32.32=0x%016llX)",
              cps, static_cast<unsigned long long>(cycles_per_sample_fp_));
}

APU::~APU() = default;

void APU::Reset() {
  psg_.Reset();
  opm_.Reset();

  cpu_cycle_accum_fp_ = 0;
  master_vol_ = 0xFF;
  audio_pan_ = 0xFF;

  total_frames_generated_ = 0;
  underrun_count_ = 0;
  overflow_count_ = 0;
  cpu_cycle_counter_ = 0;

  last_write_head_ = 0;
  last_write_count_ = 0;
  last_writes_ = {};
}

void APU::RecordWrite(uint16_t port, uint8_t value) {
  auto& w = last_writes_[static_cast<size_t>(last_write_head_)];
  w.cpu_cycle_timestamp = cpu_cycle_counter_;
  w.port = port;
  w.value = value;
  last_write_head_ = (last_write_head_ + 1) % kMaxLastWrites;
  if (last_write_count_ < kMaxLastWrites) last_write_count_++;
}

void APU::IO_Write(uint8_t port, uint8_t value, uint64_t cpu_cycle) {
  cpu_cycle_counter_ = cpu_cycle;
  RecordWrite(port, value);

  if (port == 0x60) {
    // PSG data write
    psg_.WriteData(value);
    return;
  }

  if (port == 0x70) {
    // OPM address latch
    opm_.WriteAddress(value);
    return;
  }

  if (port == 0x71) {
    // OPM data write
    opm_.WriteData(value);
    return;
  }

  // Master controls
  if (port == 0x7C) {
    master_vol_ = value;
    return;
  }

  if (port == 0x7D) {
    audio_pan_ = value;
    return;
  }
}

uint8_t APU::IO_Read(uint8_t port) {
  if (port == 0x71) {
    // OPM status read
    return opm_.ReadStatus();
  }

  // Master volume
  if (port == 0x7C) {
    return master_vol_;
  }

  // Audio pan
  if (port == 0x7D) {
    return audio_pan_;
  }

  // All other ports in APU range return 0xFF
  return 0xFF;
}

void APU::Advance(uint32_t cpu_cycles_elapsed) {
  cpu_cycle_counter_ += cpu_cycles_elapsed;

  // Fixed-point cycle-to-sample conversion (Q32.32)
  cpu_cycle_accum_fp_ += static_cast<uint64_t>(cpu_cycles_elapsed) << 32;

  int samples_to_generate = 0;
  while (cpu_cycle_accum_fp_ >= cycles_per_sample_fp_) {
    cpu_cycle_accum_fp_ -= cycles_per_sample_fp_;
    samples_to_generate++;
  }

  if (samples_to_generate > 0) {
    GenerateFrames(samples_to_generate);
  }
}

void APU::GenerateFrames(int frames) {
  // Temporary buffers for chip output
  static constexpr int kMaxFramesBatch = 1024;

  while (frames > 0) {
    int batch = std::min(frames, kMaxFramesBatch);

    float psg_buf[kMaxFramesBatch];
    float opm_l[kMaxFramesBatch];
    float opm_r[kMaxFramesBatch];

    // Render each chip
    psg_.RenderMono(psg_buf, batch);
    opm_.RenderStereo(opm_l, opm_r, batch);

    // Mix to stereo int16
    int16_t mix_buf[kMaxFramesBatch * 2];
    float master = static_cast<float>(master_vol_) / 255.0f;

    for (int i = 0; i < batch; ++i) {
      float psg_s = psg_muted_ ? 0.0f : psg_buf[i] * psg_gain_;
      float opm_sl = opm_muted_ ? 0.0f : opm_l[i] * opm_gain_;
      float opm_sr = opm_muted_ ? 0.0f : opm_r[i] * opm_gain_;

      // PSG is mono -> equal L/R
      float left = (psg_s + opm_sl) * master;
      float right = (psg_s + opm_sr) * master;

      // Hard clamp to int16
      auto clamp16 = [](float v) -> int16_t {
        int32_t s = static_cast<int32_t>(v * 32767.0f);
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        return static_cast<int16_t>(s);
      };

      mix_buf[i * 2] = clamp16(left);
      mix_buf[i * 2 + 1] = clamp16(right);
    }

    // Push to ring buffer
    int pushed = ring_->Push(mix_buf, batch);
    if (pushed < batch) {
      overflow_count_ += static_cast<uint64_t>(batch - pushed);
    }

    total_frames_generated_ += static_cast<uint64_t>(batch);
    frames -= batch;
  }
}

int APU::PopAudioFrames(int16_t* out_interleaved_lr, int frames) {
  int popped = ring_->Pop(out_interleaved_lr, frames);
  if (popped < frames) {
    // Fill remainder with silence
    std::memset(out_interleaved_lr + popped * 2, 0,
                static_cast<size_t>(frames - popped) * 2 * sizeof(int16_t));
    if (popped == 0 && frames > 0) {
      underrun_count_++;
    }
  }
  return popped;
}

DebugState APU::GetDebugState() const {
  DebugState state;
  state.stats = GetStats();
  state.psg_muted = psg_muted_;
  state.opm_muted = opm_muted_;
  return state;
}

APUAudioStats APU::GetStats() const {
  APUAudioStats stats;
  stats.total_frames_generated = total_frames_generated_;
  stats.underruns = underrun_count_;
  stats.overflows = overflow_count_;
  stats.ring_fill_frames = ring_ ? ring_->FillFrames() : 0;
  stats.ring_capacity_frames = ring_ ? ring_->CapacityFrames() : 0;
  stats.cpu_hz = config_.cpu_hz;
  stats.psg_hz = config_.psg_hz;
  stats.opm_hz = config_.opm_hz;
  stats.sample_rate = config_.sample_rate;
  return stats;
}

void APU::GetLastWrites(APUDebugLastWrite* out, int max, int& out_count) const {
  out_count = std::min(max, last_write_count_);
  for (int i = 0; i < out_count; ++i) {
    int idx = (last_write_head_ - last_write_count_ + i + kMaxLastWrites) % kMaxLastWrites;
    out[i] = last_writes_[static_cast<size_t>(idx)];
  }
}

void APU::SetMutePSG(bool mute) {
  psg_muted_ = mute;
}

void APU::SetMuteOPM(bool mute) {
  opm_muted_ = mute;
  opm_.SetMute(mute);
}

}  // namespace sz::apu
