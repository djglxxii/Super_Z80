#ifndef SUPERZ80_DEVICES_IRQ_IRQCONTROLLER_H
#define SUPERZ80_DEVICES_IRQ_IRQCONTROLLER_H

namespace sz::irq {

struct DebugState {
  bool int_asserted = false;
};

class IRQController {
 public:
  void Reset();
  void Tick();
  bool IsIntAsserted() const;
  DebugState GetDebugState() const;

 private:
  bool int_asserted_ = false;
};

}  // namespace sz::irq

#endif
