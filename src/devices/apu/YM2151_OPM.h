#ifndef SUPERZ80_DEVICES_APU_YM2151_OPM_H
#define SUPERZ80_DEVICES_APU_YM2151_OPM_H

#include <cstdint>

namespace sz::apu {

// Simplified YM2151 (OPM) FM synthesis chip.
// Implements the register interface and basic 4-operator FM synthesis
// sufficient for Phase 12 bring-up (audible, stable output).
// 8 channels, each with 4 operators.
class YM2151_OPM {
 public:
  void Reset();
  void WriteAddress(uint8_t a);
  void WriteData(uint8_t d);
  uint8_t ReadStatus();  // returns busy flag (bit 7) + timer flags

  void SetClock(double opm_hz);
  void SetSampleRate(int sample_rate);

  // Render N stereo frames.
  void RenderStereo(float* outL, float* outR, int frames);

  void SetMute(bool mute);

 private:
  // Clock / resampling
  double clock_hz_ = 3579545.0;
  int sample_rate_ = 48000;
  double tick_accum_ = 0.0;
  double ticks_per_sample_ = 0.0;

  uint8_t addr_latch_ = 0;
  uint8_t regs_[256] = {};
  bool muted_ = false;

  // Operator state (4 operators per channel, 8 channels = 32 operators)
  static constexpr int kNumChannels = 8;
  static constexpr int kNumOperators = 32;

  struct Operator {
    uint32_t phase = 0;       // phase accumulator (Q16.16)
    uint32_t phase_inc = 0;   // phase increment per tick
    float env_level = 0.0f;   // 0.0=max, 1.0=off
    int env_state = 0;        // 0=off, 1=attack, 2=decay, 3=sustain, 4=release
    float tl = 1.0f;          // total level (attenuation, 0-1 where 0=max volume)
    int ar = 0;               // attack rate
    int d1r = 0;              // decay 1 rate
    int d2r = 0;              // decay 2 rate
    int rr = 0;               // release rate
    float d1l = 0.0f;         // decay 1 level (sustain level)
    int mul = 1;              // frequency multiplier
    int dt1 = 0;              // detune 1
  };

  struct Channel {
    Operator ops[4];
    int algorithm = 0;        // 0-7 connection algorithm
    int feedback = 0;
    float fb_out[2] = {};     // feedback memory
    bool key_on[4] = {};
    int pan = 3;              // bit0=R, bit1=L
    uint16_t fnum = 0;        // frequency number (11 bits)
    int octave = 0;           // key code octave (0-7)
    int kc = 0;               // key code
    int kf = 0;               // key fraction
  };

  Channel channels_[kNumChannels] = {};

  void RecomputeTicksPerSample();
  void UpdatePhaseInc(int ch, int op);
  void UpdateAllPhaseIncs(int ch);
  void KeyOn(int ch, int op_mask);
  void KeyOff(int ch, int op_mask);
  void StepEnvelope(Operator& op);
  float CalcOperator(Operator& op, float mod_input);
  float RenderChannel(Channel& ch);

  // Sine table
  static constexpr int kSineTableSize = 1024;
  float sine_table_[kSineTableSize] = {};
  void InitSineTable();

  // Envelope timing
  static constexpr float kEnvAttackStep = 0.02f;
  static constexpr float kEnvDecayStep = 0.001f;
  static constexpr float kEnvReleaseStep = 0.005f;
};

}  // namespace sz::apu

#endif
