#include "debugui/panels/PanelDMA.h"

#include <imgui.h>

namespace sz::debugui {

void PanelDMA::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetDMADebugState();

  ImGui::Text("Phase 8: DMA Engine + Palette DMA");
  ImGui::Separator();

  // Current DMA registers
  ImGui::Text("DMA Registers:");
  ImGui::Text("  SRC: 0x%04X", state.src);
  if (state.dst_is_palette) {
    ImGui::Text("  DST: 0x%02X (Palette byte addr)", state.dst & 0xFF);
  } else {
    ImGui::Text("  DST: 0x%04X (VRAM)", state.dst);
  }
  ImGui::Text("  LEN: 0x%04X (%d bytes)", state.len, state.len);
  ImGui::Text("  CTRL: 0x%02X", state.ctrl);
  ImGui::Text("    START: %s", (state.ctrl & 0x01) ? "1" : "0");
  ImGui::Text("    QUEUE: %s", state.queue_enabled ? "enabled" : "disabled");
  ImGui::Text("    DST_IS_PALETTE: %s", state.dst_is_palette ? "YES" : "NO");
  ImGui::Text("    BUSY: 0 (instantaneous)");

  ImGui::Separator();

  // Queued DMA state
  ImGui::Text("Queued DMA:");
  if (state.queued_valid) {
    ImGui::Text("  Valid: YES");
    ImGui::Text("  SRC: 0x%04X", state.queued_src);
    if (state.queued_dst_is_palette) {
      ImGui::Text("  DST: 0x%02X (Palette byte addr)", state.queued_dst & 0xFF);
    } else {
      ImGui::Text("  DST: 0x%04X (VRAM)", state.queued_dst);
    }
    ImGui::Text("  LEN: 0x%04X (%d bytes)", state.queued_len, state.queued_len);
    ImGui::Text("  DST_IS_PALETTE: %s", state.queued_dst_is_palette ? "YES" : "NO");
  } else {
    ImGui::Text("  Valid: NO");
  }

  ImGui::Separator();

  // Last execution tracking
  ImGui::Text("Last DMA Execution:");
  if (state.last_exec_frame >= 0) {
    ImGui::Text("  Frame: %d", state.last_exec_frame);
    ImGui::Text("  Scanline: %d", state.last_exec_scanline);
    ImGui::Text("  Was Queued: %s", state.last_trigger_was_queued ? "YES" : "NO");
    ImGui::Text("  Was Palette: %s", state.last_exec_was_palette ? "YES" : "NO");
  } else {
    ImGui::Text("  None");
  }

  ImGui::Separator();

  // Error tracking
  if (state.last_illegal_start) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                       "Last START was illegal (mid-frame, no queue)");
  }
}

}  // namespace sz::debugui
