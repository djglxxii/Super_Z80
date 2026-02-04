#ifndef SUPERZ80_DEVICES_BUS_BUS_H
#define SUPERZ80_DEVICES_BUS_BUS_H

#include <cstdint>

#include "core/types.h"
#include "devices/bus/BusTypes.h"

namespace sz::cart {
class Cartridge;
}

namespace sz::io {
class IODevice;
}

namespace sz::bus {

struct BusCounters {
    std::uint64_t memReads = 0;
    std::uint64_t memWrites = 0;
    std::uint64_t ioReads = 0;
    std::uint64_t ioWrites = 0;

    std::uint64_t romReads = 0;
    std::uint64_t romWrites = 0;
    std::uint64_t ramReads = 0;
    std::uint64_t ramWrites = 0;

    std::uint64_t openBusReads = 0;
    std::uint64_t unmappedWritesIgnored = 0;

    std::uint64_t ioReadsFF = 0;
};

struct DebugState {
  BusLastAccess last{};
  BusCounters counters{};
};

class Bus {
 public:
  using Counters = BusCounters;

  Bus(sz::cart::Cartridge& cart, sz::io::IODevice& io);

  void Reset();
  u8 Read8(u16 addr);
  void Write8(u16 addr, u8 value);
  u8 In8(u8 port);
  void Out8(u8 port, u8 value);

  const BusLastAccess& GetLastAccess() const;
  const Counters& GetCounters() const;
  DebugState GetDebugState() const;

 private:
  sz::cart::Cartridge& cart_;
  sz::io::IODevice& io_;
  u8 workRam_[0x4000]{};
  BusLastAccess last_{};
  BusCounters ctr_{};

  bool IsROM(u16 addr) const;
  bool IsWorkRAM(u16 addr) const;
};

}  // namespace sz::bus

#endif
