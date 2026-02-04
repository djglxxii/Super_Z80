#ifndef SUPERZ80_CORE_LOG_TRACE_H
#define SUPERZ80_CORE_LOG_TRACE_H

#include <string>

#include "core/types.h"

namespace sz::trace {

struct TraceEvent {
  u64 frame = 0;
  int scanline = 0;
  std::string tag;
};

class ITraceSink {
 public:
  virtual ~ITraceSink() = default;
  virtual void OnEvent(const TraceEvent& event) = 0;
};

class NullTraceSink : public ITraceSink {
 public:
  void OnEvent(const TraceEvent& event) override;
};

class Trace {
 public:
  static void SetSink(ITraceSink* sink);
  static void Emit(const TraceEvent& event);
};

}  // namespace sz::trace

#endif
