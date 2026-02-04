#include "emulator/bus/Phase1Bus.h"

#include "core/util/Assert.h"

Phase1Bus::Phase1Bus() : rom_(), wram_(0x4000, 0x00) {
}

void Phase1Bus::LoadRom(std::vector<uint8_t> rom) {
  SZ_ASSERT(rom.size() <= 0x8000);
  rom_ = std::move(rom);
}

uint8_t Phase1Bus::Read8(uint16_t addr) {
  if (addr <= 0x7FFF) {
    return addr < rom_.size() ? rom_[addr] : 0xFF;
  }
  if (addr >= 0xC000) {
    return wram_[addr - 0xC000];
  }
  return 0xFF;
}

void Phase1Bus::Write8(uint16_t addr, uint8_t value) {
  if (addr >= 0xC000) {
    wram_[addr - 0xC000] = value;
  }
}

const uint8_t* Phase1Bus::GetRamPtrForDebug() const {
  return wram_.data();
}

uint32_t Phase1Bus::GetRamSizeForDebug() const {
  return static_cast<uint32_t>(wram_.size());
}
