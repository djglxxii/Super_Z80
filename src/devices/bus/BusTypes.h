#ifndef SUPERZ80_DEVICES_BUS_BUSTYPES_H
#define SUPERZ80_DEVICES_BUS_BUSTYPES_H

#include <cstdint>

namespace sz::bus {

enum class BusAccessKind : std::uint8_t { Mem, IO };
enum class BusAccessRW : std::uint8_t { Read, Write };
enum class BusTarget : std::uint8_t { ROM, WorkRAM, OpenBus, IO };

struct BusLastAccess {
  BusAccessKind kind{};
  BusAccessRW rw{};
  std::uint16_t addr = 0;
  std::uint8_t value = 0;
  BusTarget target{};
};

}  // namespace sz::bus

#endif
