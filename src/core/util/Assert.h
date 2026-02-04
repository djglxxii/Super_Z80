#ifndef SUPERZ80_CORE_UTIL_ASSERT_H
#define SUPERZ80_CORE_UTIL_ASSERT_H

#include <cstdlib>

#include "core/log/Logger.h"

#define SZ_ASSERT(cond) \
  do { \
    if (!(cond)) { \
      SZ_LOG_ERROR("Assertion failed: %s (%s:%d)", #cond, __FILE__, __LINE__); \
      std::abort(); \
    } \
  } while (0)

#define SZ_ASSERT_MSG(cond, msg) \
  do { \
    if (!(cond)) { \
      SZ_LOG_ERROR("Assertion failed: %s (%s:%d): %s", #cond, __FILE__, __LINE__, msg); \
      std::abort(); \
    } \
  } while (0)

#endif
