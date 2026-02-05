#include "devices/apu/PCM2Ch.h"

#include <cstring>

namespace sz::apu {

void PCM2Ch::Reset() {
  for (int i = 0; i < 2; ++i) {
    channels_[i] = {};
  }
}

void PCM2Ch::AttachROM(const uint8_t* rom_data, size_t rom_size) {
  rom_data_ = rom_data;
  rom_size_ = rom_size;
}

void PCM2Ch::WriteReg(int ch, int index, uint8_t v) {
  if (ch < 0 || ch > 1) return;
  auto& c = channels_[ch];

  switch (index) {
    case 0:  // START_LO
      c.regs.start_lo = v;
      break;
    case 1:  // START_HI
      c.regs.start_hi = v;
      break;
    case 2:  // LEN
      c.regs.len = v;
      break;
    case 3:  // VOL
      c.regs.vol = v;
      break;
    case 4:  // CTRL
    {
      bool new_trigger = (v & 0x01) != 0;
      bool old_trigger = c.prev_trigger;
      c.regs.ctrl = v & 0x03;  // store TRIGGER + LOOP bits
      c.prev_trigger = new_trigger;

      // Edge-triggered: 0->1 transition on TRIGGER bit
      if (new_trigger && !old_trigger) {
        uint16_t start = static_cast<uint16_t>(c.regs.start_lo) |
                         (static_cast<uint16_t>(c.regs.start_hi) << 8);
        c.cur_addr = start;
        // LEN is 8-bit, representing byte count. 0 means 256 bytes.
        c.remaining = (c.regs.len == 0) ? 256 : c.regs.len;
        c.active = true;
      }
      break;
    }
  }
}

uint8_t PCM2Ch::ReadReg(int ch, int index) const {
  if (ch < 0 || ch > 1) return 0xFF;
  const auto& c = channels_[ch];

  switch (index) {
    case 0: return c.regs.start_lo;
    case 1: return c.regs.start_hi;
    case 2: return c.regs.len;
    case 3: return c.regs.vol;
    case 4: {
      // CTRL with BUSY reflected in bit 7
      uint8_t val = c.regs.ctrl & 0x03;
      if (c.active) val |= 0x80;  // BUSY flag
      return val;
    }
    default: return 0xFF;
  }
}

void PCM2Ch::RenderMono(float* out, int frames) {
  std::memset(out, 0, static_cast<size_t>(frames) * sizeof(float));

  for (int ch = 0; ch < 2; ++ch) {
    auto& c = channels_[ch];
    if (!c.active) continue;

    float vol_scale = static_cast<float>(c.regs.vol) / 255.0f;

    for (int i = 0; i < frames; ++i) {
      if (!c.active) break;

      // Fetch byte from ROM
      float sample = 0.0f;
      if (rom_data_ && c.cur_addr < rom_size_) {
        uint8_t raw = rom_data_[c.cur_addr];
        // Convert unsigned 8-bit to signed centered [-1, +1]
        sample = (static_cast<float>(raw) - 128.0f) / 128.0f;
      }

      out[i] += sample * vol_scale;
      c.cur_addr++;
      c.remaining--;

      if (c.remaining == 0) {
        // Check LOOP
        if (c.regs.ctrl & 0x02) {
          uint16_t start = static_cast<uint16_t>(c.regs.start_lo) |
                           (static_cast<uint16_t>(c.regs.start_hi) << 8);
          c.cur_addr = start;
          c.remaining = (c.regs.len == 0) ? 256 : c.regs.len;
        } else {
          c.active = false;
        }
      }
    }
  }
}

}  // namespace sz::apu
