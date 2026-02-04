#pragma once
#include "emulator/bus/IBus.h"
#include <cstdint>
#include <vector>

class Phase1Bus final : public IBus {
public:
  Phase1Bus();

  void LoadRom(std::vector<uint8_t> rom);

  uint8_t Read8(uint16_t addr) override;
  void Write8(uint16_t addr, uint8_t value) override;
  uint8_t In8(uint8_t port) override;
  void Out8(uint8_t port, uint8_t value) override;

  const uint8_t* GetRamPtrForDebug() const override;
  uint32_t GetRamSizeForDebug() const override;

private:
  std::vector<uint8_t> rom_;
  std::vector<uint8_t> wram_;
};
