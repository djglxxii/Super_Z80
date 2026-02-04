#include "debugui/panels/PanelPPU.h"

#include <imgui.h>

namespace sz::debugui {

void PanelPPU::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetPPUDebugState();
  ImGui::Text("PPU stub (no rendering logic)");
  ImGui::Text("Last scanline: %d", state.last_scanline);
}

}  // namespace sz::debugui
