#pragma once
#include <cstdint>
#include "emulator/bus/IBus.h"
#include "emulator/bus/BusTypes.h"

class Phase2Cartridge;
class IODevice;

// Phase 2 Bus: complete address decode + debug visibility + counters
class Phase2Bus : public IBus {
public:
  struct Counters {
    uint64_t memReads  = 0;
    uint64_t memWrites = 0;
    uint64_t ioReads   = 0;
    uint64_t ioWrites  = 0;

    uint64_t romReads  = 0;
    uint64_t romWrites = 0;   // should remain 0 in normal tests
    uint64_t ramReads  = 0;
    uint64_t ramWrites = 0;

    uint64_t openBusReads = 0;
    uint64_t unmappedWritesIgnored = 0;

    uint64_t ioReadsFF = 0;   // count stubbed reads returning 0xFF
  };

public:
  Phase2Bus(Phase2Cartridge& cart, IODevice& io);

  void Reset();

  // CPU-visible memory access (IBus interface):
  uint8_t Read8(uint16_t addr) override;
  void    Write8(uint16_t addr, uint8_t value) override;

  // CPU-visible I/O access:
  uint8_t In8(uint8_t port);
  void    Out8(uint8_t port, uint8_t value);

  // Debug visibility:
  const BusLastAccess& GetLastAccess() const { return last_; }
  const Counters& GetCounters() const { return ctr_; }

  // IBus debug interface
  const uint8_t* GetRamPtrForDebug() const override { return workRam_; }
  uint32_t GetRamSizeForDebug() const override { return 0x4000; }

private:
  Phase2Cartridge& cart_;
  IODevice&  io_;

  // Work RAM backing store (Phase 2: only 0xC000–0xFFFF visible).
  uint8_t workRam_[0x4000]; // 16KB backing for visible window

  BusLastAccess last_{};
  Counters ctr_{};

private:
  // Decode helpers (Phase 2 only):
  bool IsROM(uint16_t addr) const { return addr <= 0x7FFF; }      // 0x0000–0x7FFF
  bool IsWorkRAM(uint16_t addr) const { return addr >= 0xC000; }  // 0xC000–0xFFFF
  // Phase 2: 0x8000–0xBFFF treated as unmapped (future VRAM window)
};
