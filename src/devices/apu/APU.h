#ifndef SUPERZ80_DEVICES_APU_APU_H
#define SUPERZ80_DEVICES_APU_APU_H

namespace sz::apu {

struct DebugState {
  int last_cpu_tstates = 0;
};

class APU {
 public:
  void Reset();
  void Tick(int cpu_tstates_elapsed);
  DebugState GetDebugState() const;

 private:
  int last_cpu_tstates_ = 0;
};

}  // namespace sz::apu

#endif
