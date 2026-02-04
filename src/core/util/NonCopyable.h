#ifndef SUPERZ80_CORE_UTIL_NONCOPYABLE_H
#define SUPERZ80_CORE_UTIL_NONCOPYABLE_H

namespace sz::util {

class NonCopyable {
 protected:
  NonCopyable() = default;
  ~NonCopyable() = default;

 public:
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
  NonCopyable(NonCopyable&&) = delete;
  NonCopyable& operator=(NonCopyable&&) = delete;
};

}  // namespace sz::util

#endif
