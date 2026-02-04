#include "debugui/panels/PanelDMA.h"

#include <imgui.h>

namespace sz::debugui {

void PanelDMA::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetDMADebugState();
  ImGui::Text("DMA stub (no transfers yet)");
  ImGui::Text("Tick count: %d", state.ticks);
}

}  // namespace sz::debugui
