#include "debugui/panels/PanelAPU.h"

#include <imgui.h>

namespace sz::debugui {

void PanelAPU::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetAPUDebugState();
  ImGui::Text("APU stub (no audio output)");
  ImGui::Text("Last CPU tstates: %d", state.last_cpu_tstates);
}

}  // namespace sz::debugui
