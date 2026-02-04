#include "debugui/panels/PanelCartridge.h"

#include <imgui.h>

namespace sz::debugui {

void PanelCartridge::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetCartridgeDebugState();
  ImGui::Text("Cartridge stub");
  ImGui::Text("Loaded: %s", state.loaded ? "true" : "false");
}

}  // namespace sz::debugui
