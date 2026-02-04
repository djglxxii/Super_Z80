#include "devices/io/IODevice.h"

namespace sz::io {

void IODevice::Reset() {
}

std::uint8_t IODevice::In(std::uint8_t /*port*/) {
  return 0xFF;
}

void IODevice::Out(std::uint8_t /*port*/, std::uint8_t /*value*/) {
}

}  // namespace sz::io
