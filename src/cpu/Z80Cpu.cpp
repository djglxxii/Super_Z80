#include "cpu/Z80Cpu.h"
#include "devices/bus/Bus.h"
#include "core/util/Assert.h"

extern "C" {
#include "z80ex.h"
}

namespace {
constexpr uint8_t kMaxInsnBytes = 4;
}

namespace sz::cpu {

struct Z80CpuCallbacks {
  static Z80EX_BYTE MemRead(Z80EX_CONTEXT* /*cpu*/, Z80EX_WORD addr, int /*m1_state*/, void* user_data) {
    auto* self = static_cast<Z80Cpu*>(user_data);
    uint8_t value = self->bus_.Read8(static_cast<uint16_t>(addr));

    // Capture instruction bytes for debug
    if (self->capture_active_ && self->capture_len_ < kMaxInsnBytes &&
        static_cast<uint16_t>(addr) == self->capture_expected_addr_) {
      self->dbg_.last.bytes[self->capture_len_++] = value;
      ++self->capture_expected_addr_;
    }
    return value;
  }

  static void MemWrite(Z80EX_CONTEXT* /*cpu*/, Z80EX_WORD addr, Z80EX_BYTE value, void* user_data) {
    auto* self = static_cast<Z80Cpu*>(user_data);
    self->bus_.Write8(static_cast<uint16_t>(addr), static_cast<uint8_t>(value));
  }

  static Z80EX_BYTE PortRead(Z80EX_CONTEXT* /*cpu*/, Z80EX_WORD port, void* user_data) {
    auto* self = static_cast<Z80Cpu*>(user_data);
    return self->bus_.In8(static_cast<uint8_t>(port & 0xFF));
  }

  static void PortWrite(Z80EX_CONTEXT* /*cpu*/, Z80EX_WORD port, Z80EX_BYTE value, void* user_data) {
    auto* self = static_cast<Z80Cpu*>(user_data);
    self->bus_.Out8(static_cast<uint8_t>(port & 0xFF), static_cast<uint8_t>(value));
  }

  static Z80EX_BYTE IntRead(Z80EX_CONTEXT* /*cpu*/, void* /*user_data*/) {
    // IM 1 uses vector 0xFF, IM 2 would read from data bus
    return 0xFF;
  }
};

Z80Cpu::Z80Cpu(sz::bus::Bus& bus) : bus_(bus) {
  cpu_ = z80ex_create(
      Z80CpuCallbacks::MemRead, this,
      Z80CpuCallbacks::MemWrite, this,
      Z80CpuCallbacks::PortRead, this,
      Z80CpuCallbacks::PortWrite, this,
      Z80CpuCallbacks::IntRead, this);
  SZ_ASSERT(cpu_ != nullptr);
  Reset();
}

Z80Cpu::~Z80Cpu() {
  if (cpu_ != nullptr) {
    z80ex_destroy(static_cast<Z80EX_CONTEXT*>(cpu_));
    cpu_ = nullptr;
  }
}

void Z80Cpu::Reset() {
  SZ_ASSERT(cpu_ != nullptr);
  z80ex_reset(static_cast<Z80EX_CONTEXT*>(cpu_));
  int_line_ = false;
  dbg_ = {};
  capture_active_ = false;
  capture_expected_addr_ = 0;
  capture_len_ = 0;
  RefreshDebugRegs();
}

u32 Z80Cpu::Step(u32 tstates_budget) {
  SZ_ASSERT(cpu_ != nullptr);
  if (tstates_budget == 0) {
    return 0;
  }

  auto* ctx = static_cast<Z80EX_CONTEXT*>(cpu_);
  u32 consumed = 0;

  while (consumed < tstates_budget) {
    // Check for interrupt before each instruction
    if (int_line_ && z80ex_int_possible(ctx)) {
      u32 int_tstates = static_cast<u32>(z80ex_int(ctx));
      consumed += int_tstates;
      dbg_.total_tstates += int_tstates;
      RefreshDebugRegs();

      // After servicing interrupt, int_line may still be asserted
      // The ISR should ACK the interrupt which will clear int_line_
      if (consumed >= tstates_budget) {
        break;
      }
    }

    // Setup instruction capture
    capture_active_ = true;
    capture_len_ = 0;
    u16 pc_before = static_cast<u16>(z80ex_get_reg(ctx, regPC));
    capture_expected_addr_ = pc_before;

    dbg_.last.pc = pc_before;
    dbg_.last.len = 0;
    dbg_.last.tstates = 0;
    for (u8 i = 0; i < kMaxInsnBytes; ++i) {
      dbg_.last.bytes[i] = 0x00;
    }

    // Execute one instruction (may be multi-byte prefix)
    u32 instr_tstates = 0;
    do {
      instr_tstates += static_cast<u32>(z80ex_step(ctx));
    } while (z80ex_last_op_type(ctx) != 0);

    capture_active_ = false;

    // Update debug state
    dbg_.last.len = capture_len_;
    dbg_.last.tstates = static_cast<u8>(instr_tstates & 0xFF);
    dbg_.total_tstates += instr_tstates;

    consumed += instr_tstates;
    RefreshDebugRegs();

    if (consumed >= tstates_budget) {
      break;
    }
  }

  return consumed;
}

void Z80Cpu::SetIntLine(bool asserted) {
  int_line_ = asserted;
  dbg_.int_line = asserted;
}

DebugState Z80Cpu::GetDebugState() const {
  return dbg_;
}

void Z80Cpu::RefreshDebugRegs() {
  SZ_ASSERT(cpu_ != nullptr);
  auto* ctx = static_cast<Z80EX_CONTEXT*>(cpu_);

  dbg_.regs.pc = static_cast<u16>(z80ex_get_reg(ctx, regPC));
  dbg_.regs.sp = static_cast<u16>(z80ex_get_reg(ctx, regSP));
  dbg_.regs.af = static_cast<u16>(z80ex_get_reg(ctx, regAF));
  dbg_.regs.bc = static_cast<u16>(z80ex_get_reg(ctx, regBC));
  dbg_.regs.de = static_cast<u16>(z80ex_get_reg(ctx, regDE));
  dbg_.regs.hl = static_cast<u16>(z80ex_get_reg(ctx, regHL));
  dbg_.regs.af2 = static_cast<u16>(z80ex_get_reg(ctx, regAF_));
  dbg_.regs.bc2 = static_cast<u16>(z80ex_get_reg(ctx, regBC_));
  dbg_.regs.de2 = static_cast<u16>(z80ex_get_reg(ctx, regDE_));
  dbg_.regs.hl2 = static_cast<u16>(z80ex_get_reg(ctx, regHL_));
  dbg_.regs.ix = static_cast<u16>(z80ex_get_reg(ctx, regIX));
  dbg_.regs.iy = static_cast<u16>(z80ex_get_reg(ctx, regIY));
  dbg_.regs.i = static_cast<u8>(z80ex_get_reg(ctx, regI));
  dbg_.regs.r = static_cast<u8>(z80ex_get_reg(ctx, regR));
  dbg_.regs.iff1 = z80ex_get_reg(ctx, regIFF1) != 0;
  dbg_.regs.iff2 = z80ex_get_reg(ctx, regIFF2) != 0;
  dbg_.regs.im = static_cast<u8>(z80ex_get_reg(ctx, regIM));
}

}  // namespace sz::cpu
