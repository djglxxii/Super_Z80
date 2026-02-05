#include "debugui/panels/PanelDiagnostics.h"

#include <imgui.h>

namespace sz::debugui {

void PanelDiagnostics::Draw(const sz::console::SuperZ80Console& console) {
  // Get debug states
  auto sched_state = console.GetSchedulerDebugState();
  auto irq_state = console.GetIRQDebugState();
  auto ppu_state = console.GetPPUDebugState();
  const auto& bus = console.GetBus();

  // Read RAM counters from diagnostic ROM
  // FRAME_COUNTER @ 0xC000 (16-bit, little-endian)
  // VBLANK_COUNT @ 0xC002 (16-bit, little-endian)
  // ISR_ENTRY_COUNT @ 0xC004 (16-bit, little-endian)
  u16 rom_frame_counter = bus.ReadWramDirect(0x0000) | (bus.ReadWramDirect(0x0001) << 8);
  u16 rom_vblank_count = bus.ReadWramDirect(0x0002) | (bus.ReadWramDirect(0x0003) << 8);
  u16 rom_isr_count = bus.ReadWramDirect(0x0004) | (bus.ReadWramDirect(0x0005) << 8);
  u8 rom_scratch = bus.ReadWramDirect(0x0006);

  ImGui::Text("Diagnostic ROM Validation");
  ImGui::Separator();

  // Frame counters section
  if (ImGui::CollapsingHeader("Frame Counters", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Columns(2, nullptr, false);

    ImGui::Text("Emulator Frame:");
    ImGui::NextColumn();
    ImGui::Text("%llu", static_cast<unsigned long long>(sched_state.frame_counter));
    ImGui::NextColumn();

    ImGui::Text("ROM FRAME_COUNTER (0xC000):");
    ImGui::NextColumn();
    ImGui::Text("%u", rom_frame_counter);
    ImGui::NextColumn();

    ImGui::Text("ROM VBLANK_COUNT (0xC002):");
    ImGui::NextColumn();
    ImGui::Text("%u", rom_vblank_count);
    ImGui::NextColumn();

    ImGui::Text("ROM ISR_ENTRY_COUNT (0xC004):");
    ImGui::NextColumn();
    ImGui::Text("%u", rom_isr_count);
    ImGui::NextColumn();

    ImGui::Text("ROM SCRATCH (0xC006):");
    ImGui::NextColumn();
    ImGui::Text("%02X (%s)", rom_scratch, (rom_scratch & 1) ? "Blue" : "Red");
    ImGui::NextColumn();

    ImGui::Columns(1);
  }

  // IRQ/VBlank section
  if (ImGui::CollapsingHeader("IRQ/VBlank Status", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Columns(2, nullptr, false);

    ImGui::Text("Current Scanline:");
    ImGui::NextColumn();
    ImGui::Text("%d", sched_state.current_scanline);
    ImGui::NextColumn();

    ImGui::Text("VBlank Flag (PPU):");
    ImGui::NextColumn();
    bool vblank = ppu_state.vblank_flag;
    if (vblank) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "TRUE");
    } else {
      ImGui::Text("FALSE");
    }
    ImGui::NextColumn();

    ImGui::Text("IRQ Pending Bits:");
    ImGui::NextColumn();
    ImGui::Text("%02X", irq_state.pending);
    ImGui::SameLine();
    if (irq_state.pending & 0x01) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[VBlank]");
    }
    ImGui::NextColumn();

    ImGui::Text("IRQ Enable Mask:");
    ImGui::NextColumn();
    ImGui::Text("%02X", irq_state.enable);
    ImGui::NextColumn();

    ImGui::Text("/INT Line:");
    ImGui::NextColumn();
    if (irq_state.int_line) {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ASSERTED");
    } else {
      ImGui::Text("Not asserted");
    }
    ImGui::NextColumn();

    ImGui::Text("Last VBlank Scanline:");
    ImGui::NextColumn();
    ImGui::Text("%d", irq_state.last_vblank_scanline);
    if (irq_state.last_vblank_scanline == 192) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "(OK)");
    } else if (irq_state.last_vblank_scanline != 0) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "(ERROR!)");
    }
    ImGui::NextColumn();

    ImGui::Columns(1);
  }

  // Self-check validation
  if (ImGui::CollapsingHeader("Self-Check Validation", ImGuiTreeNodeFlags_DefaultOpen)) {
    u64 current_frame = sched_state.frame_counter;

    // Skip first few frames for warm-up
    if (current_frame > 10 && current_frame != prev_frame_) {
      u16 isr_delta = rom_isr_count - prev_isr_count_;
      u16 vblank_delta = rom_vblank_count - prev_vblank_count_;
      u64 frame_delta = current_frame - prev_frame_;

      // Check for double IRQs
      if (isr_delta > frame_delta) {
        ++double_irq_count_;
        stable_ = false;
      }

      // Check for missed IRQs
      if (isr_delta < frame_delta && frame_delta < 10) {
        ++missed_irq_count_;
        stable_ = false;
      }

      ++total_checks_;
      prev_frame_ = current_frame;
      prev_isr_count_ = rom_isr_count;
      prev_vblank_count_ = rom_vblank_count;
    } else if (current_frame <= 10) {
      // Warm-up period
      prev_frame_ = current_frame;
      prev_isr_count_ = rom_isr_count;
      prev_vblank_count_ = rom_vblank_count;
    }

    ImGui::Columns(2, nullptr, false);

    ImGui::Text("Total Checks:");
    ImGui::NextColumn();
    ImGui::Text("%d", total_checks_);
    ImGui::NextColumn();

    ImGui::Text("Double IRQs Detected:");
    ImGui::NextColumn();
    if (double_irq_count_ > 0) {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%d", double_irq_count_);
    } else {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "0");
    }
    ImGui::NextColumn();

    ImGui::Text("Missed IRQs Detected:");
    ImGui::NextColumn();
    if (missed_irq_count_ > 0) {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%d", missed_irq_count_);
    } else {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "0");
    }
    ImGui::NextColumn();

    ImGui::Text("Status:");
    ImGui::NextColumn();
    if (stable_ && total_checks_ > 100) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "STABLE");
    } else if (!stable_) {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "UNSTABLE");
    } else {
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Warming up...");
    }
    ImGui::NextColumn();

    ImGui::Columns(1);

    if (ImGui::Button("Reset Checks")) {
      double_irq_count_ = 0;
      missed_irq_count_ = 0;
      total_checks_ = 0;
      stable_ = true;
    }
  }

  // Bus statistics
  if (ImGui::CollapsingHeader("Bus Statistics")) {
    auto bus_state = console.GetBusDebugState();
    ImGui::Columns(2, nullptr, false);

    ImGui::Text("ROM Loaded:");
    ImGui::NextColumn();
    ImGui::Text("%s (%zu bytes)", bus_state.rom_loaded ? "Yes" : "No", bus_state.rom_size);
    ImGui::NextColumn();

    ImGui::Text("Memory Reads:");
    ImGui::NextColumn();
    ImGui::Text("%llu", static_cast<unsigned long long>(bus_state.mem_reads));
    ImGui::NextColumn();

    ImGui::Text("Memory Writes:");
    ImGui::NextColumn();
    ImGui::Text("%llu", static_cast<unsigned long long>(bus_state.mem_writes));
    ImGui::NextColumn();

    ImGui::Text("I/O Reads:");
    ImGui::NextColumn();
    ImGui::Text("%llu", static_cast<unsigned long long>(bus_state.io_reads));
    ImGui::NextColumn();

    ImGui::Text("I/O Writes:");
    ImGui::NextColumn();
    ImGui::Text("%llu", static_cast<unsigned long long>(bus_state.io_writes));
    ImGui::NextColumn();

    ImGui::Columns(1);
  }
}

}  // namespace sz::debugui
