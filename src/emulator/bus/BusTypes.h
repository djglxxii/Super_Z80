#pragma once
#include <cstdint>

enum class BusAccessKind : uint8_t { Mem, IO };
enum class BusAccessRW   : uint8_t { Read, Write };
enum class BusTarget     : uint8_t { ROM, WorkRAM, OpenBus, IO /*, VRAM (future) */ };

struct BusLastAccess {
  BusAccessKind kind;
  BusAccessRW   rw;
  uint16_t      addr;     // memory address or port (zero-extended)
  uint8_t       value;    // read result or written value
  BusTarget     target;
};
