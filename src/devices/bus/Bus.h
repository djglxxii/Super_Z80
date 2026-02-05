#ifndef SUPERZ80_DEVICES_BUS_BUS_H
#define SUPERZ80_DEVICES_BUS_BUS_H

#include "core/types.h"

#include <array>
#include <cstdint>
#include <vector>

namespace sz::irq {
class IRQController;
}

namespace sz::ppu {
class PPU;
}

namespace sz::dma {
class DMAEngine;
}

namespace sz::bus {

struct DebugState {
  bool rom_loaded = false;
  size_t rom_size = 0;
  u64 mem_reads = 0;
  u64 mem_writes = 0;
  u64 io_reads = 0;
  u64 io_writes = 0;
};

class Bus {
 public:
  // Phase 9: Memory map constants
  static constexpr u16 kRomStart = 0x0000;
  static constexpr u16 kRomEnd = 0x7FFF;
  static constexpr u16 kWramStart = 0xC000;
  static constexpr u16 kWramEnd = 0xFFFF;
  static constexpr size_t kWramSize = 0x4000;  // 16KB Work RAM

  void Reset();
  u8 Read8(u16 addr);
  void Write8(u16 addr, u8 value);
  u8 In8(u8 port);
  void Out8(u8 port, u8 value);
  DebugState GetDebugState() const;

  // Phase 9: ROM loading
  bool LoadRom(const std::vector<u8>& rom_data);
  bool LoadRomFromFile(const char* path);
  bool IsRomLoaded() const { return !rom_.empty(); }

  // Phase 9: Debug access to RAM
  const u8* GetWramPtr() const { return wram_.data(); }
  size_t GetWramSize() const { return kWramSize; }
  u8 ReadWramDirect(u16 offset) const;

  // Wire IRQController for I/O port access
  void SetIRQController(sz::irq::IRQController* irq) { irq_ = irq; }

  // Phase 5: Wire PPU for VDP_STATUS port access
  void SetPPU(sz::ppu::PPU* ppu) { ppu_ = ppu; }

  // Phase 6: Wire DMAEngine for DMA I/O port access
  void SetDMAEngine(sz::dma::DMAEngine* dma) { dma_ = dma; }

 private:
  sz::irq::IRQController* irq_ = nullptr;
  sz::ppu::PPU* ppu_ = nullptr;
  sz::dma::DMAEngine* dma_ = nullptr;

  // Phase 9: Memory
  std::vector<u8> rom_;                       // ROM (up to 32KB)
  std::array<u8, kWramSize> wram_{};          // Work RAM (16KB)

  // Debug counters
  u64 mem_reads_ = 0;
  u64 mem_writes_ = 0;
  u64 io_reads_ = 0;
  u64 io_writes_ = 0;
};

}  // namespace sz::bus

#endif
