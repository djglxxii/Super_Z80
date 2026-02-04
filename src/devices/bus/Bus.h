#ifndef SUPERZ80_DEVICES_BUS_BUS_H
#define SUPERZ80_DEVICES_BUS_BUS_H

#include "core/types.h"

namespace sz::bus {

struct DebugState {
  bool placeholder = true;
};

class Bus {
 public:
  void Reset();
  u8 Read8(u16 addr);
  void Write8(u16 addr, u8 value);
  u8 In8(u8 port);
  void Out8(u8 port, u8 value);
  DebugState GetDebugState() const;
};

}  // namespace sz::bus

#endif
