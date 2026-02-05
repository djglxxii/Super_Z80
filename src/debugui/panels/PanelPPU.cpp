#include "debugui/panels/PanelPPU.h"

#include <imgui.h>

namespace sz::debugui {

void PanelPPU::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetPPUDebugState();
  ImGui::Text("PPU stub (no rendering logic)");
  ImGui::Text("Last scanline: %d", state.last_scanline);
  ImGui::Separator();
  ImGui::Text("Phase 5: VBlank Timing");
  ImGui::Text("VBlank flag: %s", state.vblank_flag ? "TRUE" : "FALSE");
  ImGui::Text("Last VBlank latch scanline: %u", state.last_vblank_latch_scanline);
  ImGui::Text("VBlank latch count: %llu", static_cast<unsigned long long>(state.vblank_latch_count));
}

}  // namespace sz::debugui
