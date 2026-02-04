#include "emulator/io/IODevice.h"

void IODevice::Reset() {
  // Phase 2: no state to reset
}

uint8_t IODevice::In(uint8_t /*port*/) {
  // Phase 2: all I/O reads return 0xFF
  return 0xFF;
}

void IODevice::Out(uint8_t /*port*/, uint8_t /*value*/) {
  // Phase 2: all I/O writes ignored
}
