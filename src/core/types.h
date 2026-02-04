#ifndef SUPERZ80_CORE_TYPES_H
#define SUPERZ80_CORE_TYPES_H

#include <cstdint>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

constexpr int kScreenWidth = 256;
constexpr int kScreenHeight = 192;
constexpr int kTotalScanlines = 262;
constexpr int kVBlankStartScanline = 192;

// Timing constants (per super_z80_emulation_timing_model.md)
constexpr double kMasterCrystalHz = 21'477'270.0;
constexpr double kCpuHz = kMasterCrystalHz / 4.0;  // 5'369'317.5 Hz
constexpr double kLineHz = 15'734.264;
constexpr double kCpuCyclesPerLine = kCpuHz / kLineHz;  // ~341.3364...

#endif
