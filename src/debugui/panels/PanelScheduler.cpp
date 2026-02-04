#include "debugui/panels/PanelScheduler.h"

#include <imgui.h>

namespace sz::debugui {

void PanelScheduler::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetSchedulerDebugState();
  ImGui::Text("Scheduler stub (scanline-based)");
  ImGui::Text("Frame: %llu", static_cast<unsigned long long>(state.frame));
  ImGui::Text("Scanline: %d", state.scanline);
  ImGui::Text("CPU budget: %d", state.cpu_budget_tstates);
}

}  // namespace sz::debugui
