#include "app/App.h"

#include <algorithm>

#include <SDL.h>

#include "core/config.h"
#include "core/log/Logger.h"
#include "core/types.h"
#include "core/util/Assert.h"

namespace sz::app {

namespace {
constexpr u32 kBarColors[8] = {
    0xFFFF0000u, 0xFFFF8000u, 0xFFFFFF00u, 0xFF00FF00u,
    0xFF00FFFFu, 0xFF0000FFu, 0xFF8000FFu, 0xFFFFFFFFu};
}

App::App(const AppConfig& config) : config_(config) {
}

int App::Run() {
  SZ_LOG_INFO("%s v%d.%d.%d", SUPERZ80_APP_NAME, SUPERZ80_VERSION_MAJOR,
              SUPERZ80_VERSION_MINOR, SUPERZ80_VERSION_PATCH);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    SZ_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
    return 1;
  }

  if (!sdl_.Init(SUPERZ80_APP_NAME, kScreenWidth, kScreenHeight, config_.scale)) {
    SDL_Quit();
    return 1;
  }

  if (!console_.PowerOn()) {
    SDL_Quit();
    return 1;
  }
  console_.Reset();

#if defined(SUPERZ80_ENABLE_IMGUI)
  if (config_.enable_imgui) {
    debug_ui_.Init(sdl_.GetWindow(), sdl_.GetRenderer());
  }
#endif

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
#if defined(SUPERZ80_ENABLE_IMGUI)
      if (config_.enable_imgui) {
        debug_ui_.ProcessEvent(&event);
      }
#endif
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
        running = false;
      }
    }

    console_.SetHostButtons(input_.ReadButtons());
    console_.StepFrame();

    auto& framebuffer = console_.GetFramebufferMutable();
    FillTestPattern(framebuffer, console_.GetDebugState().frame);

    presenter_.Present(sdl_, framebuffer);

#if defined(SUPERZ80_ENABLE_IMGUI)
    if (config_.enable_imgui) {
      debug_ui_.BeginFrame();
      debug_ui_.Draw(console_);
      debug_ui_.EndFrame();
    }
#endif
  }

#if defined(SUPERZ80_ENABLE_IMGUI)
  if (config_.enable_imgui) {
    debug_ui_.Shutdown();
  }
#endif

  sdl_.Shutdown();
  SDL_Quit();
  return 0;
}

void App::FillTestPattern(sz::ppu::Framebuffer& framebuffer, u64 frame) {
  SZ_ASSERT(framebuffer.width == kScreenWidth);
  SZ_ASSERT(framebuffer.height == kScreenHeight);
  if (framebuffer.pixels.empty()) {
    return;
  }

  const int bar_width = std::max(1, framebuffer.width / 8);
  const int shift = static_cast<int>(frame % framebuffer.width);

  for (int y = 0; y < framebuffer.height; ++y) {
    for (int x = 0; x < framebuffer.width; ++x) {
      int bar_index = std::min(7, x / bar_width);
      u32 base = kBarColors[bar_index];
      u32 checker = ((x + shift) ^ (y + static_cast<int>(frame))) & 0x10 ? 0xFF202020u : 0;
      framebuffer.pixels[static_cast<size_t>(y * framebuffer.width + x)] = base ^ checker;
    }
  }
}

}  // namespace sz::app
