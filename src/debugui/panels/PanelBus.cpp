#include "debugui/panels/PanelBus.h"

#include <imgui.h>

namespace sz::debugui {

namespace {
const char* ToString(sz::bus::BusTarget target) {
  switch (target) {
    case sz::bus::BusTarget::ROM:
      return "ROM";
    case sz::bus::BusTarget::WorkRAM:
      return "WorkRAM";
    case sz::bus::BusTarget::OpenBus:
      return "OpenBus";
    case sz::bus::BusTarget::IO:
      return "IO";
  }
  return "Unknown";
}
}

void PanelBus::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetBusDebugState();
  ImGui::Text("Last: %s %s addr=0x%04X val=0x%02X target=%s",
              state.last.kind == sz::bus::BusAccessKind::Mem ? "Mem" : "IO",
              state.last.rw == sz::bus::BusAccessRW::Read ? "R" : "W",
              state.last.addr, state.last.value, ToString(state.last.target));
  ImGui::Text("Mem R/W: %llu / %llu",
              static_cast<unsigned long long>(state.counters.memReads),
              static_cast<unsigned long long>(state.counters.memWrites));
  ImGui::Text("ROM R/W: %llu / %llu",
              static_cast<unsigned long long>(state.counters.romReads),
              static_cast<unsigned long long>(state.counters.romWrites));
  ImGui::Text("RAM R/W: %llu / %llu",
              static_cast<unsigned long long>(state.counters.ramReads),
              static_cast<unsigned long long>(state.counters.ramWrites));
  ImGui::Text("Open-bus R: %llu  Unmapped W: %llu",
              static_cast<unsigned long long>(state.counters.openBusReads),
              static_cast<unsigned long long>(state.counters.unmappedWritesIgnored));
  ImGui::Text("IO R/W: %llu / %llu (reads 0xFF: %llu)",
              static_cast<unsigned long long>(state.counters.ioReads),
              static_cast<unsigned long long>(state.counters.ioWrites),
              static_cast<unsigned long long>(state.counters.ioReadsFF));
}

}  // namespace sz::debugui
