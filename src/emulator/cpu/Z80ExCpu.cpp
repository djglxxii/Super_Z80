#include "emulator/cpu/Z80ExCpu.h"

#include "core/util/Assert.h"
#include "emulator/bus/IBus.h"

extern "C" {
#include "z80ex.h"
}

namespace {
constexpr uint8_t kMaxInsnBytes = 4;
}

struct Z80ExCallbacks {
  static Z80EX_BYTE MemRead(Z80EX_CONTEXT* /*cpu*/, Z80EX_WORD addr, int /*m1_state*/, void* user_data) {
    auto* self = static_cast<Z80ExCpu*>(user_data);
    uint8_t value = self->bus_.Read8(static_cast<uint16_t>(addr));
    if (self->capture_active_ && self->capture_len_ < kMaxInsnBytes &&
        static_cast<uint16_t>(addr) == self->capture_expected_addr_) {
      self->dbg_.last.bytes[self->capture_len_++] = value;
      ++self->capture_expected_addr_;
    }
    return value;
  }

  static void MemWrite(Z80EX_CONTEXT* /*cpu*/, Z80EX_WORD addr, Z80EX_BYTE value, void* user_data) {
    auto* self = static_cast<Z80ExCpu*>(user_data);
    self->bus_.Write8(static_cast<uint16_t>(addr), static_cast<uint8_t>(value));
  }

  static Z80EX_BYTE PortRead(Z80EX_CONTEXT* /*cpu*/, Z80EX_WORD /*port*/, void* /*user_data*/) {
    return 0xFF;
  }

  static void PortWrite(Z80EX_CONTEXT* /*cpu*/, Z80EX_WORD /*port*/, Z80EX_BYTE /*value*/, void* /*user_data*/) {
  }

  static Z80EX_BYTE IntRead(Z80EX_CONTEXT* /*cpu*/, void* /*user_data*/) {
    return 0xFF;
  }
};

Z80ExCpu::Z80ExCpu(IBus& bus) : bus_(bus) {
  cpu_ = z80ex_create(
      Z80ExCallbacks::MemRead, this,
      Z80ExCallbacks::MemWrite, this,
      Z80ExCallbacks::PortRead, this,
      Z80ExCallbacks::PortWrite, this,
      Z80ExCallbacks::IntRead, this);
  SZ_ASSERT(cpu_ != nullptr);
  Reset();
}

Z80ExCpu::~Z80ExCpu() {
  if (cpu_ != nullptr) {
    z80ex_destroy(static_cast<Z80EX_CONTEXT*>(cpu_));
    cpu_ = nullptr;
  }
}

void Z80ExCpu::Reset() {
  SZ_ASSERT(cpu_ != nullptr);
  z80ex_reset(static_cast<Z80EX_CONTEXT*>(cpu_));
  int_line_ = false;
  dbg_ = {};
  capture_active_ = false;
  capture_expected_addr_ = 0;
  capture_len_ = 0;
  RefreshDebugRegs();
}

uint32_t Z80ExCpu::RunTStates(uint32_t tstate_budget) {
  SZ_ASSERT(cpu_ != nullptr);
  if (tstate_budget == 0) {
    return 0;
  }

  uint32_t consumed = 0;
  while (consumed < tstate_budget) {
    capture_active_ = true;
    capture_len_ = 0;
    uint16_t pc_before = static_cast<uint16_t>(z80ex_get_reg(static_cast<Z80EX_CONTEXT*>(cpu_), regPC));
    capture_expected_addr_ = pc_before;

    dbg_.last.pc = pc_before;
    dbg_.last.len = 0;
    dbg_.last.tstates = 0;
    for (uint8_t i = 0; i < kMaxInsnBytes; ++i) {
      dbg_.last.bytes[i] = 0x00;
    }

    uint32_t instr_tstates = 0;
    do {
      instr_tstates += static_cast<uint32_t>(z80ex_step(static_cast<Z80EX_CONTEXT*>(cpu_)));
    } while (z80ex_last_op_type(static_cast<Z80EX_CONTEXT*>(cpu_)) != 0);

    capture_active_ = false;

    dbg_.last.len = capture_len_;
    dbg_.last.tstates = static_cast<uint8_t>(instr_tstates & 0xFF);

    consumed += instr_tstates;
    dbg_.total_tstates += instr_tstates;

    RefreshDebugRegs();

    if (consumed >= tstate_budget) {
      break;
    }
  }

  return consumed;
}

void Z80ExCpu::SetIntLine(bool asserted) {
  SZ_ASSERT(!asserted);
  int_line_ = false;
}

Z80DebugState Z80ExCpu::GetDebugState() const {
  return dbg_;
}

void Z80ExCpu::RefreshDebugRegs() {
  auto* ctx = static_cast<Z80EX_CONTEXT*>(cpu_);
  dbg_.regs.pc = static_cast<uint16_t>(z80ex_get_reg(ctx, regPC));
  dbg_.regs.sp = static_cast<uint16_t>(z80ex_get_reg(ctx, regSP));
  dbg_.regs.af = static_cast<uint16_t>(z80ex_get_reg(ctx, regAF));
  dbg_.regs.bc = static_cast<uint16_t>(z80ex_get_reg(ctx, regBC));
  dbg_.regs.de = static_cast<uint16_t>(z80ex_get_reg(ctx, regDE));
  dbg_.regs.hl = static_cast<uint16_t>(z80ex_get_reg(ctx, regHL));
  dbg_.regs.af2 = static_cast<uint16_t>(z80ex_get_reg(ctx, regAF_));
  dbg_.regs.bc2 = static_cast<uint16_t>(z80ex_get_reg(ctx, regBC_));
  dbg_.regs.de2 = static_cast<uint16_t>(z80ex_get_reg(ctx, regDE_));
  dbg_.regs.hl2 = static_cast<uint16_t>(z80ex_get_reg(ctx, regHL_));
  dbg_.regs.ix = static_cast<uint16_t>(z80ex_get_reg(ctx, regIX));
  dbg_.regs.iy = static_cast<uint16_t>(z80ex_get_reg(ctx, regIY));
  dbg_.regs.i = static_cast<uint8_t>(z80ex_get_reg(ctx, regI));
  dbg_.regs.r = static_cast<uint8_t>(z80ex_get_reg(ctx, regR));
  dbg_.regs.iff1 = z80ex_get_reg(ctx, regIFF1) != 0;
  dbg_.regs.iff2 = z80ex_get_reg(ctx, regIFF2) != 0;
  dbg_.regs.im = static_cast<uint8_t>(z80ex_get_reg(ctx, regIM));
}
