#ifndef SUPERZ80_DEVICES_CART_CARTRIDGE_H
#define SUPERZ80_DEVICES_CART_CARTRIDGE_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sz::cart {

struct DebugState {
  bool loaded = false;
  std::size_t rom_size = 0;
  std::uint8_t bank0 = 0;
};

class Cartridge {
 public:
  void LoadROM(const std::uint8_t* data, std::size_t size);

  void Reset();
  std::uint8_t ReadROM(std::uint16_t addr) const;
  void WriteROM(std::uint16_t addr, std::uint8_t value);

  void WriteMapperPort(std::uint8_t port, std::uint8_t value);
  std::uint8_t ReadMapperPort(std::uint8_t port) const;

  DebugState GetDebugState() const;

 private:
  std::vector<std::uint8_t> rom_;
  bool loaded_ = false;
  std::uint8_t bank0_ = 0;
};

}  // namespace sz::cart

#endif
