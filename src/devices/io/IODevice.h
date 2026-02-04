#ifndef SUPERZ80_DEVICES_IO_IODEVICE_H
#define SUPERZ80_DEVICES_IO_IODEVICE_H

#include <cstdint>

namespace sz::io {

class IODevice {
 public:
  void Reset();
  std::uint8_t In(std::uint8_t port);
  void Out(std::uint8_t port, std::uint8_t value);
};

}  // namespace sz::io

#endif
