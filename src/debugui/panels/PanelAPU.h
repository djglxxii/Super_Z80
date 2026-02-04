#ifndef SUPERZ80_DEBUGUI_PANELS_PANELAPU_H
#define SUPERZ80_DEBUGUI_PANELS_PANELAPU_H

#include "console/SuperZ80Console.h"

namespace sz::debugui {

class PanelAPU {
 public:
  void Draw(const sz::console::SuperZ80Console& console);
};

}  // namespace sz::debugui

#endif
