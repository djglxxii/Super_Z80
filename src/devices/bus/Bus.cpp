#include "devices/bus/Bus.h"

#include <cstring>

#include "devices/cart/Cartridge.h"
#include "devices/io/IODevice.h"

namespace sz::bus {

Bus::Bus(sz::cart::Cartridge& cart, sz::io::IODevice& io) : cart_(cart), io_(io) {
  Reset();
}

void Bus::Reset() {
  cart_.Reset();
  io_.Reset();
  std::memset(workRam_, 0, sizeof(workRam_));
  last_ = {};
  ctr_ = {};
}

u8 Bus::Read8(u16 addr) {
  ++ctr_.memReads;
  if (IsROM(addr)) {
    u8 value = cart_.ReadROM(addr);
    ++ctr_.romReads;
    last_ = {BusAccessKind::Mem, BusAccessRW::Read, addr, value, BusTarget::ROM};
    return value;
  }
  if (IsWorkRAM(addr)) {
    u16 offset = static_cast<u16>(addr - 0xC000);
    u8 value = workRam_[offset];
    ++ctr_.ramReads;
    last_ = {BusAccessKind::Mem, BusAccessRW::Read, addr, value, BusTarget::WorkRAM};
    return value;
  }
  // Phase 2: 0x8000-0xBFFF is treated as unmapped (VRAM window in Phase 3+).
  ++ctr_.openBusReads;
  last_ = {BusAccessKind::Mem, BusAccessRW::Read, addr, 0xFF, BusTarget::OpenBus};
  return 0xFF;
}

void Bus::Write8(u16 addr, u8 value) {
  ++ctr_.memWrites;
  if (IsROM(addr)) {
    cart_.WriteROM(addr, value);
    ++ctr_.romWrites;
    last_ = {BusAccessKind::Mem, BusAccessRW::Write, addr, value, BusTarget::ROM};
    return;
  }
  if (IsWorkRAM(addr)) {
    u16 offset = static_cast<u16>(addr - 0xC000);
    workRam_[offset] = value;
    ++ctr_.ramWrites;
    last_ = {BusAccessKind::Mem, BusAccessRW::Write, addr, value, BusTarget::WorkRAM};
    return;
  }
  // Phase 2: 0x8000-0xBFFF is treated as unmapped (VRAM window in Phase 3+).
  ++ctr_.unmappedWritesIgnored;
  last_ = {BusAccessKind::Mem, BusAccessRW::Write, addr, value, BusTarget::OpenBus};
}

u8 Bus::In8(u8 port) {
  ++ctr_.ioReads;
  u8 value = io_.In(port);
  if (value == 0xFF) {
    ++ctr_.ioReadsFF;
  }
  last_ = {BusAccessKind::IO, BusAccessRW::Read, port, value, BusTarget::IO};
  return value;
}

void Bus::Out8(u8 port, u8 value) {
  ++ctr_.ioWrites;
  io_.Out(port, value);
  last_ = {BusAccessKind::IO, BusAccessRW::Write, port, value, BusTarget::IO};
}

const BusLastAccess& Bus::GetLastAccess() const {
  return last_;
}

const Bus::Counters& Bus::GetCounters() const {
  return ctr_;
}

DebugState Bus::GetDebugState() const {
  DebugState state;
  state.last = last_;
  state.counters = ctr_;
  return state;
}

bool Bus::IsROM(u16 addr) const {
  return addr <= 0x7FFF;
}

bool Bus::IsWorkRAM(u16 addr) const {
  return addr >= 0xC000;
}

}  // namespace sz::bus
