#include "devices/bus/Bus.h"

namespace sz::bus {

void Bus::Reset() {
}

u8 Bus::Read8(u16 /*addr*/) {
  return 0xFF;
}

void Bus::Write8(u16 /*addr*/, u8 /*value*/) {
}

u8 Bus::In8(u8 /*port*/) {
  return 0xFF;
}

void Bus::Out8(u8 /*port*/, u8 /*value*/) {
}

DebugState Bus::GetDebugState() const {
  return DebugState{};
}

}  // namespace sz::bus
