#include "debugui/panels/PanelScheduler.h"

#include <imgui.h>

namespace sz::debugui {

void PanelScheduler::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetSchedulerDebugState();

  ImGui::Text("Phase 3: Scheduler & Scanline Timing");
  ImGui::Separator();

  // Core state
  ImGui::Text("Frame: %llu", static_cast<unsigned long long>(state.frame_counter));
  ImGui::Text("Scanline: %u / 261", state.current_scanline);
  ImGui::Text("VBlank: %s", state.vblank_flag ? "YES" : "NO");
  ImGui::Separator();

  // Cycle accounting
  ImGui::Text("Cycles this scanline: %u", state.cycles_this_scanline);
  ImGui::Text("CPU cycles per line: %.4f", state.cpu_cycles_per_line);
  ImGui::Text("Accumulator: %.6f", state.cpu_cycle_accumulator);
  ImGui::Text("Total CPU T-states: %llu", static_cast<unsigned long long>(state.total_cpu_tstates_executed));
  ImGui::Separator();

  // Validation
  u64 lines_total = state.frame_counter * 262 + state.current_scanline;
  double expected = static_cast<double>(lines_total) * state.cpu_cycles_per_line;
  double actual = static_cast<double>(state.total_cpu_tstates_executed) + state.cpu_cycle_accumulator;
  double error = actual - expected;
  ImGui::Text("Lines executed: %llu", static_cast<unsigned long long>(lines_total));
  ImGui::Text("Expected cycles: %.2f", expected);
  ImGui::Text("Actual cycles: %.2f", actual);
  ImGui::Text("Error: %.9f", error);

  ImGui::Separator();
  ImGui::Text("Ring buffer: visible in log or future UI");
}

}  // namespace sz::debugui
