#ifndef SUPERZ80_APP_SDLHOST_H
#define SUPERZ80_APP_SDLHOST_H

#include <string>

#include <SDL.h>

namespace sz::app {

class SDLHost {
 public:
  bool Init(const std::string& title, int width, int height, int scale);
  void Shutdown();

  SDL_Window* GetWindow() const;
  SDL_Renderer* GetRenderer() const;
  SDL_Texture* GetTexture() const;
  int GetScale() const;

 private:
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* texture_ = nullptr;
  int scale_ = 1;
};

}  // namespace sz::app

#endif
