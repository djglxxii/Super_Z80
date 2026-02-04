#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/log/Logger.h"
#include "core/util/Assert.h"
#include "emulator/bus/Phase1Bus.h"
#include "emulator/cpu/Z80ExCpu.h"

namespace {
constexpr uint16_t kLoopStartPc = 0x0008;
constexpr uint32_t kLoopIterations = 0x100;
constexpr uint32_t kSetupInstructions = 3;
constexpr uint32_t kLoopInstructions = 4;
constexpr size_t kTraceLimit = 32;
constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

uint64_t HashByte(uint64_t hash, uint8_t value) {
  hash ^= value;
  return hash * kFnvPrime;
}

uint64_t HashU16(uint64_t hash, uint16_t value) {
  hash = HashByte(hash, static_cast<uint8_t>(value & 0xFF));
  hash = HashByte(hash, static_cast<uint8_t>((value >> 8) & 0xFF));
  return hash;
}

uint64_t HashU64(uint64_t hash, uint64_t value) {
  for (int i = 0; i < 8; ++i) {
    hash = HashByte(hash, static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
  }
  return hash;
}

std::vector<uint8_t> LoadRom() {
  std::filesystem::path rom_path = std::filesystem::path(SUPERZ80_SOURCE_DIR)
                                  / "tests" / "roms" / "phase1_determinism.bin";
  std::ifstream file(rom_path, std::ios::binary);
  SZ_ASSERT_MSG(file.good(), "Failed to open phase1_determinism.bin");
  std::vector<uint8_t> rom(std::istreambuf_iterator<char>(file), {});
  SZ_ASSERT(!rom.empty());
  return rom;
}

struct TraceEntry {
  uint16_t pc = 0;
  uint16_t sp = 0;
  uint16_t af = 0;
  uint16_t hl = 0;
  uint8_t last_len = 0;
  uint8_t last_bytes[4] = {0, 0, 0, 0};
  uint8_t last_tstates = 0;
  uint64_t total_tstates = 0;

  bool operator==(const TraceEntry& other) const {
    if (pc != other.pc || sp != other.sp || af != other.af || hl != other.hl ||
        last_len != other.last_len || last_tstates != other.last_tstates ||
        total_tstates != other.total_tstates) {
      return false;
    }
    for (int i = 0; i < 4; ++i) {
      if (last_bytes[i] != other.last_bytes[i]) {
        return false;
      }
    }
    return true;
  }
};

struct RunResult {
  uint64_t hash = 0;
  std::vector<TraceEntry> trace;
  Z80DebugState final_state{};
};

RunResult RunOnce(const std::vector<uint8_t>& rom) {
  Phase1Bus bus;
  bus.LoadRom(rom);
  Z80ExCpu cpu(bus);
  cpu.Reset();

  RunResult result;
  result.trace.reserve(kTraceLimit);

  uint32_t total_instructions = kSetupInstructions + (kLoopIterations * kLoopInstructions);
  for (uint32_t i = 0; i < total_instructions; ++i) {
    uint32_t consumed = cpu.RunTStates(1);
    SZ_ASSERT(consumed > 0);

    if (result.trace.size() < kTraceLimit) {
      Z80DebugState state = cpu.GetDebugState();
      TraceEntry entry;
      entry.pc = state.regs.pc;
      entry.sp = state.regs.sp;
      entry.af = state.regs.af;
      entry.hl = state.regs.hl;
      entry.last_len = state.last.len;
      entry.last_tstates = state.last.tstates;
      entry.total_tstates = state.total_tstates;
      for (int j = 0; j < 4; ++j) {
        entry.last_bytes[j] = state.last.bytes[j];
      }
      result.trace.push_back(entry);
    }
  }

  result.final_state = cpu.GetDebugState();

  const uint8_t* ram = bus.GetRamPtrForDebug();
  SZ_ASSERT(ram != nullptr);
  SZ_ASSERT(bus.GetRamSizeForDebug() >= 0x4000);

  for (size_t i = 0; i < 0x100; ++i) {
    uint8_t expected = static_cast<uint8_t>(i + 1);
    SZ_ASSERT(ram[i] == expected);
  }

  uint8_t a = static_cast<uint8_t>(result.final_state.regs.af >> 8);
  SZ_ASSERT(a == 0x00);
  SZ_ASSERT(result.final_state.regs.hl == 0xC100);
  SZ_ASSERT(result.final_state.regs.sp == 0xFFFE);
  SZ_ASSERT(result.final_state.regs.pc == kLoopStartPc);

  uint64_t hash = kFnvOffsetBasis;
  for (size_t i = 0; i < 0x100; ++i) {
    hash = HashByte(hash, ram[i]);
  }
  hash = HashU16(hash, result.final_state.regs.pc);
  hash = HashU16(hash, result.final_state.regs.sp);
  hash = HashU16(hash, result.final_state.regs.af);
  hash = HashU16(hash, result.final_state.regs.hl);
  hash = HashU64(hash, result.final_state.total_tstates);
  result.hash = hash;

  return result;
}

void CompareTraces(const std::vector<TraceEntry>& a, const std::vector<TraceEntry>& b) {
  SZ_ASSERT(a.size() == b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    SZ_ASSERT(a[i] == b[i]);
  }
}
}

int main() {
  std::vector<uint8_t> rom = LoadRom();

  RunResult first = RunOnce(rom);
  RunResult second = RunOnce(rom);

  CompareTraces(first.trace, second.trace);
  SZ_ASSERT(first.hash == second.hash);

  SZ_LOG_INFO("cpu_phase1_determinism: OK (hash=0x%llx)",
              static_cast<unsigned long long>(first.hash));
  return 0;
}
