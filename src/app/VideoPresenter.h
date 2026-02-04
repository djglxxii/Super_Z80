#ifndef SUPERZ80_APP_VIDEOPRESENTER_H
#define SUPERZ80_APP_VIDEOPRESENTER_H

#include "app/SDLHost.h"
#include "devices/ppu/PPU.h"

namespace sz::app {

class VideoPresenter {
 public:
  void Present(SDLHost& host, const sz::ppu::Framebuffer& framebuffer);
};

}  // namespace sz::app

#endif
