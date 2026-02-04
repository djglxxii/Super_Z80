#ifndef SUPERZ80_CPU_Z80CPUSTUB_H
#define SUPERZ80_CPU_Z80CPUSTUB_H

namespace sz::cpu {

struct DebugState {
  int last_budget = 0;
};

class Z80CpuStub {
 public:
  void Reset();
  void Step(int tstates_budget);
  DebugState GetDebugState() const;

 private:
  int last_budget_ = 0;
};

}  // namespace sz::cpu

#endif
