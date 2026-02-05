#ifndef SUPERZ80_DEVICES_APU_SN76489_PSG_H
#define SUPERZ80_DEVICES_APU_SN76489_PSG_H

#include <cstdint>

namespace sz::apu {

// SN76489-style Programmable Sound Generator.
// 3 tone channels + 1 noise channel, mono output.
// MAME-derived implementation for accuracy.
class SN76489_PSG {
 public:
  void Reset();
  void WriteData(uint8_t v);

  void SetClock(double psg_hz);
  void SetSampleRate(int sample_rate);

  // Render N mono samples into output buffer.
  void RenderMono(float* out, int frames);

 private:
  void UpdateToneChannel(int ch);
  void UpdateNoiseChannel();

  // Clock / sample rate
  double clock_hz_ = 3579545.0;
  int sample_rate_ = 48000;

  // Fractional accumulator for chip tick stepping
  double tick_accum_ = 0.0;
  double ticks_per_sample_ = 0.0;

  // Register state
  int latch_channel_ = 0;   // 0-3 (3 = noise)
  bool latch_is_volume_ = false;

  // Tone channels (0-2)
  uint16_t tone_reg_[3] = {0, 0, 0};    // 10-bit period
  uint8_t  vol_reg_[4] = {0xF, 0xF, 0xF, 0xF};  // 0=max, 0xF=off
  int16_t  tone_counter_[3] = {0, 0, 0};
  int8_t   tone_output_[3] = {1, 1, 1};  // +1 or -1

  // Noise channel
  uint8_t  noise_reg_ = 0;   // NF1 NF0 FB (3 bits)
  int16_t  noise_counter_ = 0;
  uint16_t noise_lfsr_ = 0x8000;
  int8_t   noise_output_ = 1;

  // Volume table (attenuation in dB: 0, -2, -4, ..., -28, off)
  static constexpr int kVolTableSize = 16;
  float vol_table_[kVolTableSize] = {};

  void InitVolTable();
  void RecomputeTicksPerSample();
};

}  // namespace sz::apu

#endif
