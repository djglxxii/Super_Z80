#include "debugui/panels/PanelIRQ.h"

#include <imgui.h>

namespace sz::debugui {

void PanelIRQ::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetIRQDebugState();

  ImGui::Text("Phase 4: IRQ Infrastructure");
  ImGui::Separator();

  ImGui::Text("Scanline: %u", state.scanline);
  ImGui::Text("/INT Line: %s", state.int_line_asserted ? "ASSERTED" : "deasserted");

  ImGui::Separator();
  ImGui::Text("Pending Bits: 0x%02X", state.pending);
  ImGui::BulletText("VBlank:      %s", (state.pending & 0x01) ? "SET" : "---");
  ImGui::BulletText("Timer:       %s", (state.pending & 0x02) ? "SET" : "---");
  ImGui::BulletText("Scanline:    %s", (state.pending & 0x04) ? "SET" : "---");
  ImGui::BulletText("SprOverflow: %s", (state.pending & 0x08) ? "SET" : "---");
  ImGui::BulletText("DmaDone:     %s", (state.pending & 0x10) ? "SET" : "---");

  ImGui::Separator();
  ImGui::Text("Enable Mask: 0x%02X", state.enable);
  ImGui::BulletText("VBlank:      %s", (state.enable & 0x01) ? "ENABLED" : "disabled");
  ImGui::BulletText("Timer:       %s", (state.enable & 0x02) ? "ENABLED" : "disabled");
  ImGui::BulletText("Scanline:    %s", (state.enable & 0x04) ? "ENABLED" : "disabled");
  ImGui::BulletText("SprOverflow: %s", (state.enable & 0x08) ? "ENABLED" : "disabled");
  ImGui::BulletText("DmaDone:     %s", (state.enable & 0x10) ? "ENABLED" : "disabled");

  ImGui::Separator();
  ImGui::Text("Counters:");
  ImGui::BulletText("Synthetic Fire Count: %llu", static_cast<unsigned long long>(state.synthetic_fire_count));
  ImGui::BulletText("ISR Entry Count:      %llu", static_cast<unsigned long long>(state.isr_entry_count));
}

}  // namespace sz::debugui
