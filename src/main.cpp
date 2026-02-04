#include <cstdlib>
#include <string>

#include "app/App.h"
#include "core/log/Logger.h"

namespace {
int ParseScale(const char* value) {
  try {
    int scale = std::stoi(value);
    return scale > 0 ? scale : 1;
  } catch (...) {
    return 1;
  }
}
}

int main(int argc, char** argv) {
  sz::app::AppConfig config;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--scale" && i + 1 < argc) {
      config.scale = ParseScale(argv[++i]);
    } else if (arg == "--no-imgui") {
      config.enable_imgui = false;
    } else if (arg == "--help") {
      SZ_LOG_INFO("Usage: superz80_app [--scale N] [--no-imgui]");
      return 0;
    }
  }

  sz::app::App app(config);
  return app.Run();
}
