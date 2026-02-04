#ifndef SUPERZ80_APP_TIMESOURCE_H
#define SUPERZ80_APP_TIMESOURCE_H

#include <cstdint>

namespace sz::app {

class TimeSource {
 public:
  std::uint64_t NowTicks() const;
};

}  // namespace sz::app

#endif
