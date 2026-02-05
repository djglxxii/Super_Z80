#ifndef SUPERZ80_DEVICES_APU_PCM2CH_H
#define SUPERZ80_DEVICES_APU_PCM2CH_H

#include <cstddef>
#include <cstdint>

namespace sz::apu {

struct PCMChannelRegs {
  uint8_t start_lo = 0;
  uint8_t start_hi = 0;
  uint8_t len = 0;
  uint8_t vol = 0;
  uint8_t ctrl = 0;  // bit0=TRIGGER, bit1=LOOP, bit7=BUSY(read-only)
};

// 2-channel PCM playback engine.
// Trigger-based one-shot playback from cartridge ROM.
// 8-bit unsigned samples, 1 byte per output sample.
class PCM2Ch {
 public:
  void Reset();
  void AttachROM(const uint8_t* rom_data, size_t rom_size);

  // Write to a channel register (ch: 0 or 1, index: 0-4 for START_LO/HI/LEN/VOL/CTRL)
  void WriteReg(int ch, int index, uint8_t v);

  // Read CTRL register with BUSY reflection
  uint8_t ReadReg(int ch, int index) const;

  // Render mono output for N frames (one sample per frame per channel, mixed)
  void RenderMono(float* out, int frames);

 private:
  struct ChannelState {
    PCMChannelRegs regs;
    uint16_t cur_addr = 0;
    uint16_t remaining = 0;  // in bytes/samples
    bool active = false;
    bool prev_trigger = false;
  };

  ChannelState channels_[2] = {};
  const uint8_t* rom_data_ = nullptr;
  size_t rom_size_ = 0;
};

}  // namespace sz::apu

#endif
