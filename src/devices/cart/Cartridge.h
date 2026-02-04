#ifndef SUPERZ80_DEVICES_CART_CARTRIDGE_H
#define SUPERZ80_DEVICES_CART_CARTRIDGE_H

#include <string>

namespace sz::cart {

struct DebugState {
  bool loaded = false;
};

class Cartridge {
 public:
  bool LoadFromFile(const std::string& path);
  void Reset();
  DebugState GetDebugState() const;

 private:
  bool loaded_ = false;
};

}  // namespace sz::cart

#endif
