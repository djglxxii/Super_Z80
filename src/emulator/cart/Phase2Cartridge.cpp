#include "emulator/cart/Phase2Cartridge.h"

void Phase2Cartridge::LoadROM(const uint8_t* data, size_t size) {
  rom_.assign(data, data + size);
}

void Phase2Cartridge::Reset() {
  // Phase 2: enforce bank 0 mapped at reset vector
  currentBank_ = 0;
}

uint8_t Phase2Cartridge::ReadROM(uint16_t addr) const {
  // Phase 2: addr is in 0x0000–0x7FFF
  // Bank 0 always mapped (no banking logic yet)
  if (addr < rom_.size()) {
    return rom_[addr];
  }
  // Read past ROM size returns 0xFF (open-bus behavior)
  return 0xFF;
}

void Phase2Cartridge::WriteROM(uint16_t /*addr*/, uint8_t /*value*/) {
  // Phase 2: ROM writes ignored (counter incremented by Bus)
}

void Phase2Cartridge::WriteMapperPort(uint8_t /*port*/, uint8_t /*value*/) {
  // Phase 2 stub: future mapper control
}

uint8_t Phase2Cartridge::ReadMapperPort(uint8_t /*port*/) const {
  // Phase 2 stub: return 0xFF
  return 0xFF;
}
