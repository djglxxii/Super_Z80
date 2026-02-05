#include "devices/bus/Bus.h"
#include "devices/dma/DMAEngine.h"
#include "devices/irq/IRQController.h"
#include "devices/ppu/PPU.h"

namespace sz::bus {

void Bus::Reset() {
}

u8 Bus::Read8(u16 /*addr*/) {
  return 0xFF;
}

void Bus::Write8(u16 /*addr*/, u8 /*value*/) {
}

u8 Bus::In8(u8 port) {
  // Phase 7: PPU I/O ports (0x10-0x1F)
  if (port >= 0x10 && port <= 0x1F) {
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
      // All other ports return 0xFF (stubbed/unmapped)
      return 0xFF;
  }
}

void Bus::Out8(u8 port, u8 value) {
  // Phase 7: PPU I/O ports (0x10-0x1F)
  if (port >= 0x10 && port <= 0x1F) {
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
      // All other ports are ignored (writes have no effect)
      return;
  }
}

DebugState Bus::GetDebugState() const {
  return DebugState{};
}

}  // namespace sz::bus
