#include "app/App.h"

#include <SDL.h>

#include "core/config.h"
#include "core/log/Logger.h"
#include "core/types.h"
#include "devices/apu/APU.h"

namespace sz::app {

App::App(const AppConfig& config) : config_(config) {
}

int App::Run() {
  SZ_LOG_INFO("%s v%d.%d.%d", SUPERZ80_APP_NAME, SUPERZ80_VERSION_MAJOR,
              SUPERZ80_VERSION_MINOR, SUPERZ80_VERSION_PATCH);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) != 0) {
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

  // Phase 9: Load ROM if specified
  if (config_.rom_path != nullptr) {
    if (!console_.LoadRom(config_.rom_path)) {
      SZ_LOG_ERROR("Failed to load ROM: %s", config_.rom_path);
      SDL_Quit();
      return 1;
    }
  }

  console_.Reset();

  // Phase 12: Open SDL audio device and start playback
  {
    SDLAudioSpecOut audio_spec{};
    if (audio_.Open(&console_.GetAPU(), sz::apu::kAPU_SAMPLE_RATE, audio_spec)) {
      audio_.Start();
    } else {
      SZ_LOG_WARN("App: Audio device failed to open; continuing without audio");
    }
  }

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

    // Phase 7: PPU now renders directly to framebuffer
    const auto& framebuffer = console_.GetFramebuffer();

#if defined(SUPERZ80_ENABLE_IMGUI)
    if (config_.enable_imgui) {
      // Don't present yet - let ImGui draw on top first
      presenter_.Present(sdl_, framebuffer, false);
      debug_ui_.BeginFrame();
      debug_ui_.Draw(console_);
      debug_ui_.EndFrame();
      // Now present everything together
      SDL_RenderPresent(sdl_.GetRenderer());
    } else {
      presenter_.Present(sdl_, framebuffer, true);
    }
#else
    presenter_.Present(sdl_, framebuffer, true);
#endif
  }

#if defined(SUPERZ80_ENABLE_IMGUI)
  if (config_.enable_imgui) {
    debug_ui_.Shutdown();
  }
#endif

  // Phase 12: Stop and close audio before shutting down
  audio_.Stop();
  audio_.Close();

  sdl_.Shutdown();
  SDL_Quit();
  return 0;
}

}  // namespace sz::app
