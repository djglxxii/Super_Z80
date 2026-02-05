#ifndef SUPERZ80_APP_APP_H
#define SUPERZ80_APP_APP_H

#include "app/InputHost.h"
#include "app/SDLHost.h"
#include "app/TimeSource.h"
#include "app/VideoPresenter.h"
#include "console/SuperZ80Console.h"

#if defined(SUPERZ80_ENABLE_IMGUI)
#include "debugui/DebugUI.h"
#endif

namespace sz::app {

struct AppConfig {
  int scale = 3;
  bool enable_imgui = true;
  const char* rom_path = nullptr;  // Phase 9: ROM to load
};

class App {
 public:
  explicit App(const AppConfig& config);
  int Run();

 private:
  AppConfig config_{};
  SDLHost sdl_{};
  VideoPresenter presenter_{};
  InputHost input_{};
  TimeSource time_{};
  sz::console::SuperZ80Console console_{};

#if defined(SUPERZ80_ENABLE_IMGUI)
  sz::debugui::DebugUI debug_ui_{};
#endif
};

}  // namespace sz::app

#endif
