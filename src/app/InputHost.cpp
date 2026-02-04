#include "app/InputHost.h"

#include <SDL.h>

namespace sz::app {

sz::input::HostButtons InputHost::ReadButtons() const {
  sz::input::HostButtons buttons;
  const Uint8* state = SDL_GetKeyboardState(nullptr);

  buttons.up = state[SDL_SCANCODE_UP] != 0;
  buttons.down = state[SDL_SCANCODE_DOWN] != 0;
  buttons.left = state[SDL_SCANCODE_LEFT] != 0;
  buttons.right = state[SDL_SCANCODE_RIGHT] != 0;
  buttons.a = state[SDL_SCANCODE_Z] != 0;
  buttons.b = state[SDL_SCANCODE_X] != 0;
  buttons.start = state[SDL_SCANCODE_RETURN] != 0;
  buttons.select = state[SDL_SCANCODE_RSHIFT] != 0;

  return buttons;
}

}  // namespace sz::app
