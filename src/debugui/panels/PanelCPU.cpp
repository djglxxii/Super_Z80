#include "debugui/panels/PanelCPU.h"

#include <imgui.h>

namespace sz::debugui {

void PanelCPU::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetCpuDebugState();
  ImGui::Text("CPU stub (no execution)");
  ImGui::Text("Last budget: %d", state.last_budget);
}

}  // namespace sz::debugui
