#include "emulator/bus/Phase2Bus.h"
#include "emulator/cart/Phase2Cartridge.h"
#include "emulator/io/IODevice.h"
#include <cstring>

Phase2Bus::Phase2Bus(Phase2Cartridge& cart, IODevice& io)
  : cart_(cart), io_(io) {
  std::memset(workRam_, 0, sizeof(workRam_));
}

void Phase2Bus::Reset() {
  // Reset children
  cart_.Reset();
  io_.Reset();

  // Clear counters deterministically
  ctr_ = Counters{};

  // Clear last access deterministically
  last_ = BusLastAccess{};

  // Zero Work RAM
  std::memset(workRam_, 0, sizeof(workRam_));
}

uint8_t Phase2Bus::Read8(uint16_t addr) {
  ctr_.memReads++;

  uint8_t value;
  BusTarget target;

  if (IsROM(addr)) {
    // ROM region: 0x0000–0x7FFF
    value = cart_.ReadROM(addr);
    target = BusTarget::ROM;
    ctr_.romReads++;
  }
  else if (IsWorkRAM(addr)) {
    // Work RAM region: 0xC000–0xFFFF
    uint16_t offset = addr - 0xC000;
    value = workRam_[offset];
    target = BusTarget::WorkRAM;
    ctr_.ramReads++;
  }
  else {
    // Unmapped memory (Phase 2: 0x8000–0xBFFF treated as unmapped)
    // PHASE 2 ASSUMPTION: 0x8000–0xBFFF is unmapped (future VRAM window)
    value = 0xFF;
    target = BusTarget::OpenBus;
    ctr_.openBusReads++;
  }

  // Update last access
  last_.kind = BusAccessKind::Mem;
  last_.rw = BusAccessRW::Read;
  last_.addr = addr;
  last_.value = value;
  last_.target = target;

  return value;
}

void Phase2Bus::Write8(uint16_t addr, uint8_t value) {
  ctr_.memWrites++;

  BusTarget target;

  if (IsROM(addr)) {
    // ROM region: writes ignored but counted
    cart_.WriteROM(addr, value);
    target = BusTarget::ROM;
    ctr_.romWrites++;
  }
  else if (IsWorkRAM(addr)) {
    // Work RAM region: 0xC000–0xFFFF
    uint16_t offset = addr - 0xC000;
    workRam_[offset] = value;
    target = BusTarget::WorkRAM;
    ctr_.ramWrites++;
  }
  else {
    // Unmapped memory: write ignored
    target = BusTarget::OpenBus;
    ctr_.unmappedWritesIgnored++;
  }

  // Update last access
  last_.kind = BusAccessKind::Mem;
  last_.rw = BusAccessRW::Write;
  last_.addr = addr;
  last_.value = value;
  last_.target = target;
}

uint8_t Phase2Bus::In8(uint8_t port) {
  ctr_.ioReads++;

  // Phase 2: all I/O reads return 0xFF
  uint8_t value = io_.In(port);
  ctr_.ioReadsFF++;

  // Update last access
  last_.kind = BusAccessKind::IO;
  last_.rw = BusAccessRW::Read;
  last_.addr = port;  // zero-extended
  last_.value = value;
  last_.target = BusTarget::IO;

  return value;
}

void Phase2Bus::Out8(uint8_t port, uint8_t value) {
  ctr_.ioWrites++;

  // Phase 2: writes ignored/logged
  io_.Out(port, value);

  // Update last access
  last_.kind = BusAccessKind::IO;
  last_.rw = BusAccessRW::Write;
  last_.addr = port;  // zero-extended
  last_.value = value;
  last_.target = BusTarget::IO;
}
