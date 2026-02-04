#ifndef SUPERZ80_DEVICES_DMA_DMAENGINE_H
#define SUPERZ80_DEVICES_DMA_DMAENGINE_H

namespace sz::dma {

struct DebugState {
  int ticks = 0;
};

class DMAEngine {
 public:
  void Reset();
  void Tick();
  DebugState GetDebugState() const;

 private:
  int ticks_ = 0;
};

}  // namespace sz::dma

#endif
