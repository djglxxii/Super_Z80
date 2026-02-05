#ifndef SUPERZ80_APP_VIDEOPRESENTER_H
#define SUPERZ80_APP_VIDEOPRESENTER_H

#include "app/SDLHost.h"
#include "devices/ppu/PPU.h"

namespace sz::app {

class VideoPresenter {
 public:
  // Render framebuffer to screen. If do_present is false, caller must call SDL_RenderPresent.
  void Present(SDLHost& host, const sz::ppu::Framebuffer& framebuffer, bool do_present = true);
};

}  // namespace sz::app

#endif
