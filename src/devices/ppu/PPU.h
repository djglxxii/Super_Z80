#ifndef SUPERZ80_DEVICES_PPU_PPU_H
#define SUPERZ80_DEVICES_PPU_PPU_H

#include <vector>

#include "core/types.h"

namespace sz::ppu {

struct Framebuffer {
  std::vector<u32> pixels;
  int width = kScreenWidth;
  int height = kScreenHeight;
};

struct DebugState {
  int last_scanline = -1;
};

class PPU {
 public:
  void Reset();
  void RenderScanline(int scanline, Framebuffer& fb);
  DebugState GetDebugState() const;

 private:
  int last_scanline_ = -1;
};

}  // namespace sz::ppu

#endif
