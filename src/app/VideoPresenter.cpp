#include "app/VideoPresenter.h"

#include "core/log/Logger.h"
#include "core/types.h"

namespace sz::app {

void VideoPresenter::Present(SDLHost& host, const sz::ppu::Framebuffer& framebuffer, bool do_present) {
  SDL_Renderer* renderer = host.GetRenderer();
  SDL_Texture* texture = host.GetTexture();
  if (!renderer || !texture) {
    return;
  }

  const int pitch = framebuffer.width * static_cast<int>(sizeof(u32));
  if (SDL_UpdateTexture(texture, nullptr, framebuffer.pixels.data(), pitch) != 0) {
    SZ_LOG_WARN("SDL_UpdateTexture failed: %s", SDL_GetError());
  }

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  SDL_Rect dest{0, 0, framebuffer.width * host.GetScale(), framebuffer.height * host.GetScale()};
  SDL_RenderCopy(renderer, texture, nullptr, &dest);

  // Only present if requested (allows ImGui to draw on top before presenting)
  if (do_present) {
    SDL_RenderPresent(renderer);
  }
}

}  // namespace sz::app
