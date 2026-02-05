#include "devices/apu/SN76489_PSG.h"

#include <cmath>
#include <cstring>

namespace sz::apu {

void SN76489_PSG::InitVolTable() {
  // SN76489 volume: 0=max, 15=off
  // Each step is -2dB attenuation
  for (int i = 0; i < 15; ++i) {
    vol_table_[i] = static_cast<float>(std::pow(10.0, -0.1 * i));
  }
  vol_table_[15] = 0.0f;
}

void SN76489_PSG::RecomputeTicksPerSample() {
  if (sample_rate_ > 0) {
    // PSG internal clock divides by 16 for tone counters
    ticks_per_sample_ = (clock_hz_ / 16.0) / sample_rate_;
  }
}

void SN76489_PSG::Reset() {
  latch_channel_ = 0;
  latch_is_volume_ = false;
  tick_accum_ = 0.0;

  for (int i = 0; i < 3; ++i) {
    tone_reg_[i] = 0;
    tone_counter_[i] = 0;
    tone_output_[i] = 1;
  }
  for (int i = 0; i < 4; ++i) {
    vol_reg_[i] = 0xF;  // all channels off
  }

  noise_reg_ = 0;
  noise_counter_ = 0;
  noise_lfsr_ = 0x8000;
  noise_output_ = 1;

  InitVolTable();
  RecomputeTicksPerSample();
}

void SN76489_PSG::SetClock(double psg_hz) {
  clock_hz_ = psg_hz;
  RecomputeTicksPerSample();
}

void SN76489_PSG::SetSampleRate(int sample_rate) {
  sample_rate_ = sample_rate;
  RecomputeTicksPerSample();
}

void SN76489_PSG::WriteData(uint8_t v) {
  if (v & 0x80) {
    // Latch/data byte: 1 cc t dddd
    // cc = channel (0-3), t = type (0=tone/noise, 1=volume), dddd = data
    latch_channel_ = (v >> 5) & 0x03;
    latch_is_volume_ = (v >> 4) & 0x01;
    uint8_t data = v & 0x0F;

    if (latch_is_volume_) {
      vol_reg_[latch_channel_] = data;
    } else {
      if (latch_channel_ < 3) {
        // Tone: low 4 bits
        tone_reg_[latch_channel_] = (tone_reg_[latch_channel_] & 0x3F0) | data;
      } else {
        // Noise register
        noise_reg_ = data & 0x07;
        noise_lfsr_ = 0x8000;
      }
    }
  } else {
    // Data byte: 0 x dddddd
    uint8_t data = v & 0x3F;

    if (latch_is_volume_) {
      vol_reg_[latch_channel_] = data & 0x0F;
    } else {
      if (latch_channel_ < 3) {
        // Tone: high 6 bits
        tone_reg_[latch_channel_] = (tone_reg_[latch_channel_] & 0x00F) | (static_cast<uint16_t>(data) << 4);
      } else {
        noise_reg_ = data & 0x07;
        noise_lfsr_ = 0x8000;
      }
    }
  }
}

void SN76489_PSG::RenderMono(float* out, int frames) {
  for (int i = 0; i < frames; ++i) {
    tick_accum_ += ticks_per_sample_;
    int ticks = static_cast<int>(tick_accum_);
    tick_accum_ -= ticks;

    for (int t = 0; t < ticks; ++t) {
      // Step tone channels
      for (int ch = 0; ch < 3; ++ch) {
        tone_counter_[ch]--;
        if (tone_counter_[ch] <= 0) {
          tone_output_[ch] = -tone_output_[ch];
          tone_counter_[ch] = static_cast<int16_t>(tone_reg_[ch]);
          if (tone_counter_[ch] == 0) {
            tone_counter_[ch] = 1;
          }
        }
      }

      // Step noise channel
      noise_counter_--;
      if (noise_counter_ <= 0) {
        // Clock the LFSR
        bool feedback;
        if (noise_reg_ & 0x04) {
          // White noise: tapped feedback (bits 0 and 3 XOR, SN76489 variant)
          feedback = ((noise_lfsr_ & 0x0009) != 0) && ((noise_lfsr_ & 0x0009) != 0x0009);
        } else {
          // Periodic noise: bit 0
          feedback = noise_lfsr_ & 1;
        }
        noise_lfsr_ = (noise_lfsr_ >> 1) | (feedback ? 0x8000 : 0);
        noise_output_ = (noise_lfsr_ & 1) ? 1 : -1;

        // Noise period from register
        int period;
        switch (noise_reg_ & 0x03) {
          case 0x00: period = 0x10; break;
          case 0x01: period = 0x20; break;
          case 0x02: period = 0x40; break;
          default:   period = tone_reg_[2] ? tone_reg_[2] : 1; break;
        }
        noise_counter_ = static_cast<int16_t>(period);
      }
    }

    // Mix: sum all channels
    float sample = 0.0f;
    for (int ch = 0; ch < 3; ++ch) {
      if (tone_reg_[ch] > 0) {
        sample += static_cast<float>(tone_output_[ch]) * vol_table_[vol_reg_[ch]];
      }
    }
    sample += static_cast<float>(noise_output_) * vol_table_[vol_reg_[3]];

    out[i] = sample;
  }
}

}  // namespace sz::apu
