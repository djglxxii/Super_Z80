#include "core/log/Trace.h"

namespace sz::trace {

namespace {
NullTraceSink g_null_sink;
ITraceSink* g_sink = &g_null_sink;
}  // namespace

void NullTraceSink::OnEvent(const TraceEvent& /*event*/) {
  // no-op
}

void Trace::SetSink(ITraceSink* sink) {
  g_sink = sink ? sink : &g_null_sink;
}

void Trace::Emit(const TraceEvent& event) {
  if (g_sink) {
    g_sink->OnEvent(event);
  }
}

}  // namespace sz::trace
