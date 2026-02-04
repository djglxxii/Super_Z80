#include "devices/cart/Cartridge.h"

namespace sz::cart {

bool Cartridge::LoadFromFile(const std::string& /*path*/) {
  loaded_ = false;
  return loaded_;
}

void Cartridge::Reset() {
}

DebugState Cartridge::GetDebugState() const {
  DebugState state;
  state.loaded = loaded_;
  return state;
}

}  // namespace sz::cart
