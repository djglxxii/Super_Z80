#ifndef SUPERZ80_DEBUGUI_PANELS_PANELAPU_H
#define SUPERZ80_DEBUGUI_PANELS_PANELAPU_H

#include "console/SuperZ80Console.h"

namespace sz::debugui {

class PanelAPU {
 public:
  void Draw(sz::console::SuperZ80Console& console);

 private:
  bool psg_muted_ = false;
  bool opm_muted_ = false;
};

}  // namespace sz::debugui

#endif
