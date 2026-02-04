#ifndef SUPERZ80_DEBUGUI_PANELS_PANELIRQ_H
#define SUPERZ80_DEBUGUI_PANELS_PANELIRQ_H

#include "console/SuperZ80Console.h"

namespace sz::debugui {

class PanelIRQ {
 public:
  void Draw(const sz::console::SuperZ80Console& console);
};

}  // namespace sz::debugui

#endif
