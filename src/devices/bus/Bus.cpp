#include "devices/bus/Bus.h"
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
  // Decode I/O ports for Phase 5
  switch (port) {
    case 0x10:  // VDP_STATUS (R) - Phase 5
      if (ppu_) {
        // Bit 0: VBLANK flag (live view of vblank_flag)
        // Other bits: 0 for now (no sprites, no scanline compare, etc.)
        u8 status = 0x00;
        if (ppu_->GetVBlankFlag()) {
          status |= 0x01;  // VBlank bit
        }
        return status;
      }
      return 0xFF;

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
      // All other ports return 0xFF (stubbed/unmapped)
      return 0xFF;
  }
}

void Bus::Out8(u8 port, u8 value) {
  // Decode I/O ports for Phase 4
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
      // All other ports are ignored (writes have no effect)
      return;
  }
}

DebugState Bus::GetDebugState() const {
  return DebugState{};
}

}  // namespace sz::bus
