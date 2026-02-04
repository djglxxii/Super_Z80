#ifndef SUPERZ80_DEBUGUI_DEBUGUI_H
#define SUPERZ80_DEBUGUI_DEBUGUI_H

#include <SDL.h>

#include "console/SuperZ80Console.h"

namespace sz::debugui {

class DebugUI {
 public:
  void Init(SDL_Window* window, SDL_Renderer* renderer);
  void Shutdown();
  void ProcessEvent(const SDL_Event* event);
  void BeginFrame();
  void Draw(const sz::console::SuperZ80Console& console);
  void EndFrame(SDL_Renderer* renderer);

 private:
  bool initialized_ = false;
};

}  // namespace sz::debugui

#endif
