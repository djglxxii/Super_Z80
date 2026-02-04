#include "core/log/Logger.h"

#include <array>
#include <cstdio>
#include <mutex>

namespace sz::log {

namespace {
std::mutex g_log_mutex;
Level g_level = Level::Info;

const char* LevelToString(Level level) {
  switch (level) {
    case Level::Error:
      return "ERROR";
    case Level::Warn:
      return "WARN";
    case Level::Info:
      return "INFO";
    case Level::Debug:
      return "DEBUG";
    case Level::Trace:
      return "TRACE";
    default:
      return "LOG";
  }
}

bool ShouldLog(Level level) {
  return static_cast<int>(level) <= static_cast<int>(g_level);
}
}  // namespace

void Logger::SetLevel(Level level) {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  g_level = level;
}

Level Logger::GetLevel() {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  return g_level;
}

void Logger::Log(Level level, const char* fmt, ...) {
  std::va_list args;
  va_start(args, fmt);
  LogV(level, fmt, args);
  va_end(args);
}

void Logger::LogV(Level level, const char* fmt, std::va_list args) {
  if (!ShouldLog(level)) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_log_mutex);
  std::array<char, 2048> buffer{};
  std::vsnprintf(buffer.data(), buffer.size(), fmt, args);
  std::fprintf(level == Level::Error ? stderr : stdout, "[%s] %s\n", LevelToString(level), buffer.data());
}

}  // namespace sz::log
