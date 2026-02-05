#include "debugui/panels/PanelCPU.h"

#include <imgui.h>

namespace sz::debugui {

void PanelCPU::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetCpuDebugState();

  ImGui::Text("Z80 CPU (z80ex)");
  ImGui::Separator();

  // Registers
  if (ImGui::CollapsingHeader("Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Columns(4, nullptr, false);

    ImGui::Text("PC: %04X", state.regs.pc);
    ImGui::NextColumn();
    ImGui::Text("SP: %04X", state.regs.sp);
    ImGui::NextColumn();
    ImGui::Text("AF: %04X", state.regs.af);
    ImGui::NextColumn();
    ImGui::Text("AF': %04X", state.regs.af2);
    ImGui::NextColumn();

    ImGui::Text("BC: %04X", state.regs.bc);
    ImGui::NextColumn();
    ImGui::Text("DE: %04X", state.regs.de);
    ImGui::NextColumn();
    ImGui::Text("HL: %04X", state.regs.hl);
    ImGui::NextColumn();
    ImGui::Text("IX: %04X", state.regs.ix);
    ImGui::NextColumn();

    ImGui::Text("BC': %04X", state.regs.bc2);
    ImGui::NextColumn();
    ImGui::Text("DE': %04X", state.regs.de2);
    ImGui::NextColumn();
    ImGui::Text("HL': %04X", state.regs.hl2);
    ImGui::NextColumn();
    ImGui::Text("IY: %04X", state.regs.iy);
    ImGui::NextColumn();

    ImGui::Columns(1);
  }

  // Flags and interrupt state
  if (ImGui::CollapsingHeader("Flags & Interrupts", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Columns(4, nullptr, false);

    ImGui::Text("I: %02X", state.regs.i);
    ImGui::NextColumn();
    ImGui::Text("R: %02X", state.regs.r);
    ImGui::NextColumn();
    ImGui::Text("IM: %d", state.regs.im);
    ImGui::NextColumn();
    ImGui::Text(" ");
    ImGui::NextColumn();

    ImGui::Text("IFF1: %s", state.regs.iff1 ? "ON" : "OFF");
    ImGui::NextColumn();
    ImGui::Text("IFF2: %s", state.regs.iff2 ? "ON" : "OFF");
    ImGui::NextColumn();
    ImGui::Text("/INT: %s", state.int_line ? "LOW" : "HIGH");
    ImGui::NextColumn();
    ImGui::Text(" ");
    ImGui::NextColumn();

    ImGui::Columns(1);

    // Decode F register
    u8 f = state.regs.af & 0xFF;
    ImGui::Text("Flags: %c%c%c%c%c%c%c%c",
                (f & 0x80) ? 'S' : '-',  // Sign
                (f & 0x40) ? 'Z' : '-',  // Zero
                (f & 0x20) ? '5' : '-',  // Undocumented
                (f & 0x10) ? 'H' : '-',  // Half-carry
                (f & 0x08) ? '3' : '-',  // Undocumented
                (f & 0x04) ? 'V' : '-',  // Parity/Overflow
                (f & 0x02) ? 'N' : '-',  // Add/Subtract
                (f & 0x01) ? 'C' : '-'); // Carry
  }

  // Last instruction
  if (ImGui::CollapsingHeader("Last Instruction", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("PC: %04X", state.last.pc);
    ImGui::SameLine();
    ImGui::Text("T-states: %d", state.last.tstates);

    // Display instruction bytes
    char bytes_str[32] = {0};
    int offset = 0;
    for (u8 i = 0; i < state.last.len && i < 4; ++i) {
      offset += snprintf(bytes_str + offset, sizeof(bytes_str) - offset, "%02X ", state.last.bytes[i]);
    }
    ImGui::Text("Bytes: %s", bytes_str);
  }

  // Statistics
  if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("Total T-states: %llu", static_cast<unsigned long long>(state.total_tstates));
  }
}

}  // namespace sz::debugui
