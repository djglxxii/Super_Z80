#include "app/TimeSource.h"

#include <chrono>

namespace sz::app {

std::uint64_t TimeSource::NowTicks() const {
  auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

}  // namespace sz::app
