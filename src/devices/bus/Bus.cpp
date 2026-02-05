#include "devices/bus/Bus.h"
#include "devices/apu/APU.h"
#include "devices/dma/DMAEngine.h"
#include "devices/irq/IRQController.h"
#include "devices/ppu/PPU.h"
#include "core/log/Logger.h"

#include <cstring>
#include <fstream>

namespace sz::bus {

void Bus::Reset() {
  // Clear Work RAM
  wram_.fill(0);

  // Reset counters
  mem_reads_ = 0;
  mem_writes_ = 0;
  io_reads_ = 0;
  io_writes_ = 0;

  // Note: ROM is NOT cleared on reset - it persists
}

bool Bus::LoadRom(const std::vector<u8>& rom_data) {
  if (rom_data.empty()) {
    SZ_LOG_ERROR("Bus::LoadRom: Empty ROM data");
    return false;
  }
  if (rom_data.size() > 0x8000) {  // 32KB max
    SZ_LOG_ERROR("Bus::LoadRom: ROM too large (%zu bytes, max 32768)", rom_data.size());
    return false;
  }

  rom_ = rom_data;
  SZ_LOG_INFO("Bus::LoadRom: Loaded %zu bytes", rom_.size());
  return true;
}

bool Bus::LoadRomFromFile(const char* path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    SZ_LOG_ERROR("Bus::LoadRomFromFile: Cannot open '%s'", path);
    return false;
  }

  auto size = file.tellg();
  if (size <= 0 || size > 0x8000) {
    SZ_LOG_ERROR("Bus::LoadRomFromFile: Invalid size %lld", static_cast<long long>(size));
    return false;
  }

  file.seekg(0);
  std::vector<u8> data(static_cast<size_t>(size));
  file.read(reinterpret_cast<char*>(data.data()), size);

  return LoadRom(data);
}

u8 Bus::ReadWramDirect(u16 offset) const {
  if (offset < kWramSize) {
    return wram_[offset];
  }
  return 0xFF;
}

u8 Bus::Read8(u16 addr) {
  ++mem_reads_;

  // ROM: 0x0000 - 0x7FFF
  if (addr <= kRomEnd) {
    if (addr < rom_.size()) {
      return rom_[addr];
    }
    return 0xFF;  // Unmapped ROM area
  }

  // Work RAM: 0xC000 - 0xFFFF
  if (addr >= kWramStart) {
    return wram_[addr - kWramStart];
  }

  // 0x8000 - 0xBFFF: Unmapped (future VRAM window, etc.)
  return 0xFF;
}

void Bus::Write8(u16 addr, u8 value) {
  ++mem_writes_;

  // ROM: 0x0000 - 0x7FFF - writes ignored
  if (addr <= kRomEnd) {
    return;
  }

  // Work RAM: 0xC000 - 0xFFFF
  if (addr >= kWramStart) {
    wram_[addr - kWramStart] = value;
    return;
  }

  // 0x8000 - 0xBFFF: Unmapped - writes ignored
}

u8 Bus::In8(u8 port) {
  ++io_reads_;

  // Phase 7: PPU I/O ports (0x10-0x1F)
  // Phase 11: Sprite I/O ports (0x20-0x2F)
  if (port >= 0x10 && port <= 0x2F) {
    if (ppu_) {
      return ppu_->IoRead(port);
    }
    return 0xFF;
  }

  // Decode I/O ports
  switch (port) {
    case 0x80:  // IRQ_STATUS (R)
      if (irq_) {
        return irq_->ReadStatus();
      }
      return 0xFF;

    case 0x81:  // IRQ_ENABLE (R/W)
      if (irq_) {
        return irq_->ReadEnable();
      }
      return 0xFF;

    default:
      // Phase 6: DMA I/O ports (0x30-0x36)
      if (port >= 0x30 && port <= 0x36) {
        if (dma_) {
          return dma_->ReadReg(port);
        }
        return 0xFF;
      }
      // Phase 12: APU I/O ports (0x60-0x7D)
      if (port >= 0x60 && port <= 0x7D) {
        if (apu_) {
          return apu_->IO_Read(port);
        }
        return 0xFF;
      }
      // All other ports return 0xFF (stubbed/unmapped)
      return 0xFF;
  }
}

void Bus::Out8(u8 port, u8 value) {
  ++io_writes_;

  // Phase 7: PPU I/O ports (0x10-0x1F)
  // Phase 11: Sprite I/O ports (0x20-0x2F)
  if (port >= 0x10 && port <= 0x2F) {
    if (ppu_) {
      ppu_->IoWrite(port, value);
    }
    return;
  }

  // Decode I/O ports
  switch (port) {
    case 0x81:  // IRQ_ENABLE (R/W)
      if (irq_) {
        irq_->WriteEnable(value);
        irq_->PostCpuUpdate();  // Ensure /INT drops immediately if needed
      }
      return;

    case 0x82:  // IRQ_ACK (W)
      if (irq_) {
        irq_->Ack(value);
        irq_->PostCpuUpdate();  // Ensure /INT drops immediately if needed
      }
      return;

    default:
      // Phase 6: DMA I/O ports (0x30-0x36)
      if (port >= 0x30 && port <= 0x36) {
        if (dma_) {
          dma_->WriteReg(port, value);
        }
        return;
      }
      // Phase 12: APU I/O ports (0x60-0x7D)
      if (port >= 0x60 && port <= 0x7D) {
        if (apu_) {
          apu_->IO_Write(port, value, 0);
        }
        return;
      }
      // All other ports are ignored (writes have no effect)
      return;
  }
}

DebugState Bus::GetDebugState() const {
  DebugState state;
  state.rom_loaded = !rom_.empty();
  state.rom_size = rom_.size();
  state.mem_reads = mem_reads_;
  state.mem_writes = mem_writes_;
  state.io_reads = io_reads_;
  state.io_writes = io_writes_;
  return state;
}

bool Bus::GetIntLine() const {
  if (irq_) {
    return irq_->IntLineAsserted();
  }
  return false;
}

}  // namespace sz::bus
