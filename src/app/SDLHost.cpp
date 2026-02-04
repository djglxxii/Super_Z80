#include "app/SDLHost.h"

#include "core/log/Logger.h"

namespace sz::app {

bool SDLHost::Init(const std::string& title, int width, int height, int scale) {
  scale_ = scale;
  int window_w = width * scale_;
  int window_h = height * scale_;

  window_ = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             window_w, window_h, SDL_WINDOW_SHOWN);
  if (!window_) {
    SZ_LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
    return false;
  }

  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer_) {
    SZ_LOG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
    SDL_DestroyWindow(window_);
    window_ = nullptr;
    return false;
  }

  texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!texture_) {
    SZ_LOG_ERROR("SDL_CreateTexture failed: %s", SDL_GetError());
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    renderer_ = nullptr;
    window_ = nullptr;
    return false;
  }

  return true;
}

void SDLHost::Shutdown() {
  if (texture_) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
  }
  if (renderer_) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
}

SDL_Window* SDLHost::GetWindow() const {
  return window_;
}

SDL_Renderer* SDLHost::GetRenderer() const {
  return renderer_;
}

SDL_Texture* SDLHost::GetTexture() const {
  return texture_;
}

int SDLHost::GetScale() const {
  return scale_;
}

}  // namespace sz::app
