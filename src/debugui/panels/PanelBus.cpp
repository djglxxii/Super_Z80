#include "debugui/panels/PanelBus.h"

#include <imgui.h>

namespace sz::debugui {

void PanelBus::Draw(const sz::console::SuperZ80Console& /*console*/) {
  ImGui::Text("Bus stub (no decoding yet)");
}

}  // namespace sz::debugui
