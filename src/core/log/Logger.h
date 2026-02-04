#ifndef SUPERZ80_CORE_LOG_LOGGER_H
#define SUPERZ80_CORE_LOG_LOGGER_H

#include <cstdarg>
#include <string>

namespace sz::log {

enum class Level {
  Error,
  Warn,
  Info,
  Debug,
  Trace
};

class Logger {
 public:
  static void SetLevel(Level level);
  static Level GetLevel();
  static void Log(Level level, const char* fmt, ...);
  static void LogV(Level level, const char* fmt, std::va_list args);
};

}  // namespace sz::log

#define SZ_LOG_ERROR(fmt, ...) ::sz::log::Logger::Log(::sz::log::Level::Error, fmt, ##__VA_ARGS__)
#define SZ_LOG_WARN(fmt, ...) ::sz::log::Logger::Log(::sz::log::Level::Warn, fmt, ##__VA_ARGS__)
#define SZ_LOG_INFO(fmt, ...) ::sz::log::Logger::Log(::sz::log::Level::Info, fmt, ##__VA_ARGS__)
#define SZ_LOG_DEBUG(fmt, ...) ::sz::log::Logger::Log(::sz::log::Level::Debug, fmt, ##__VA_ARGS__)
#define SZ_LOG_TRACE(fmt, ...) ::sz::log::Logger::Log(::sz::log::Level::Trace, fmt, ##__VA_ARGS__)

#endif
