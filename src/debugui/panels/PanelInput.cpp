#include "debugui/panels/PanelInput.h"

#include <imgui.h>

namespace sz::debugui {

void PanelInput::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetInputDebugState();
  ImGui::Text("Input stub");
  ImGui::Text("Up: %d  Down: %d  Left: %d  Right: %d", state.buttons.up, state.buttons.down,
              state.buttons.left, state.buttons.right);
  ImGui::Text("A: %d  B: %d  Start: %d  Select: %d", state.buttons.a, state.buttons.b,
              state.buttons.start, state.buttons.select);
}

}  // namespace sz::debugui
