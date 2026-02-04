#include "devices/cart/Cartridge.h"

namespace sz::cart {

void Cartridge::LoadROM(const std::uint8_t* data, std::size_t size) {
  rom_.assign(data, data + size);
  loaded_ = !rom_.empty();
}

void Cartridge::Reset() {
  bank0_ = 0;
}

std::uint8_t Cartridge::ReadROM(std::uint16_t addr) const {
  if (addr < rom_.size()) {
    return rom_[addr];
  }
  return 0xFF;
}

void Cartridge::WriteROM(std::uint16_t /*addr*/, std::uint8_t /*value*/) {
}

void Cartridge::WriteMapperPort(std::uint8_t /*port*/, std::uint8_t /*value*/) {
}

std::uint8_t Cartridge::ReadMapperPort(std::uint8_t /*port*/) const {
  return 0xFF;
}

DebugState Cartridge::GetDebugState() const {
  DebugState state;
  state.loaded = loaded_;
  state.rom_size = rom_.size();
  state.bank0 = bank0_;
  return state;
}

}  // namespace sz::cart
