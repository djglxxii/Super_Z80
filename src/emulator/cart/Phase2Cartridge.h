#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

// Phase 2 minimal cartridge: ROM access with bank 0 enforced at reset
class Phase2Cartridge {
public:
  void LoadROM(const uint8_t* data, size_t size);

  // Phase 2: bank 0 only, no banking logic (stub state allowed).
  void Reset();  // must force bank0 mapping defaults

  uint8_t ReadROM(uint16_t addr) const;  // addr in 0x0000–0x7FFF

  // Phase 2: writes to ROM region are ignored (but counted).
  void WriteROM(uint16_t addr, uint8_t value);

  // Phase 2 stub for future mapper-controlled behavior.
  void WriteMapperPort(uint8_t port, uint8_t value);  // stub: log/ignore
  uint8_t ReadMapperPort(uint8_t port) const;         // stub: return 0xFF

  // Debug
  size_t GetROMSize() const { return rom_.size(); }

private:
  std::vector<uint8_t> rom_;
  // Phase 2: bank state stub (always 0)
  uint8_t currentBank_ = 0;
};
