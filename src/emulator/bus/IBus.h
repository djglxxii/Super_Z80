#pragma once
#include <cstdint>

class IBus {
public:
  virtual ~IBus() = default;

  virtual uint8_t Read8(uint16_t addr) = 0;
  virtual void Write8(uint16_t addr, uint8_t value) = 0;

  virtual const uint8_t* GetRamPtrForDebug() const = 0;
  virtual uint32_t GetRamSizeForDebug() const = 0;
};
