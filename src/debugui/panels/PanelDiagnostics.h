#ifndef SUPERZ80_DEBUGUI_PANELS_PANELDIAGNOSTICS_H
#define SUPERZ80_DEBUGUI_PANELS_PANELDIAGNOSTICS_H

#include "console/SuperZ80Console.h"

namespace sz::debugui {

class PanelDiagnostics {
 public:
  void Draw(const sz::console::SuperZ80Console& console);

 private:
  // Tracking for self-check validation
  u64 prev_frame_ = 0;
  u16 prev_isr_count_ = 0;
  u16 prev_vblank_count_ = 0;

  // Statistics
  int double_irq_count_ = 0;
  int missed_irq_count_ = 0;
  int total_checks_ = 0;
  bool stable_ = true;
};

}  // namespace sz::debugui

#endif
