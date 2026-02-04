#pragma once
#include <cstdint>

// Phase 2 I/O stub: all ports return 0xFF on reads, writes are ignored/logged
class IODevice {
public:
  void Reset();

  // Phase 2 contract: unmapped ports return 0xFF; writes ignored/logged.
  uint8_t In(uint8_t port);
  void Out(uint8_t port, uint8_t value);
};
