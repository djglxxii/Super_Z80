#include "debugui/panels/PanelIRQ.h"

#include <imgui.h>

namespace sz::debugui {

void PanelIRQ::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetIRQDebugState();
  ImGui::Text("IRQ stub (no pending bits)");
  ImGui::Text("/INT asserted: %s", state.int_asserted ? "true" : "false");
}

}  // namespace sz::debugui
