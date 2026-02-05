#ifndef SUPERZ80_DEBUGUI_DEBUGUI_H
#define SUPERZ80_DEBUGUI_DEBUGUI_H

#include <SDL.h>

#include "console/SuperZ80Console.h"
#include "debugui/panels/PanelDiagnostics.h"

namespace sz::debugui {

class DebugUI {
 public:
  void Init(SDL_Window* window, SDL_Renderer* renderer);
  void Shutdown();
  void ProcessEvent(const SDL_Event* event);
  void BeginFrame();
  void Draw(sz::console::SuperZ80Console& console);
  void EndFrame();

 private:
  bool initialized_ = false;

  // Phase 9: Diagnostics panel needs persistent state
  PanelDiagnostics diag_panel_;
};

}  // namespace sz::debugui

#endif
