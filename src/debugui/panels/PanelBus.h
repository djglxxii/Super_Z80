#ifndef SUPERZ80_DEBUGUI_PANELS_PANELBUS_H
#define SUPERZ80_DEBUGUI_PANELS_PANELBUS_H

#include "console/SuperZ80Console.h"

namespace sz::debugui {

class PanelBus {
 public:
  void Draw(const sz::console::SuperZ80Console& console);
};

}  // namespace sz::debugui

#endif
