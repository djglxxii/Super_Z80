#include "debugui/panels/PanelAPU.h"

#include <imgui.h>

#include "devices/apu/APU.h"

namespace sz::debugui {

void PanelAPU::Draw(sz::console::SuperZ80Console& console) {
  auto state = console.GetAPUDebugState();
  const auto& stats = state.stats;

  // Clocking / rates
  ImGui::Text("CPU_HZ: %.1f", stats.cpu_hz);
  ImGui::SameLine();
  ImGui::Text("  PSG_HZ: %.1f", stats.psg_hz);
  ImGui::SameLine();
  ImGui::Text("  OPM_HZ: %.1f", stats.opm_hz);
  ImGui::Text("Sample Rate: %d Hz", stats.sample_rate);

  ImGui::Separator();

  // Ring buffer metrics
  ImGui::Text("Ring Buffer: %d / %d frames (%.1f%%)",
              stats.ring_fill_frames, stats.ring_capacity_frames,
              stats.ring_capacity_frames > 0
                  ? 100.0f * static_cast<float>(stats.ring_fill_frames) /
                        static_cast<float>(stats.ring_capacity_frames)
                  : 0.0f);
  ImGui::Text("Total Generated: %llu frames",
              static_cast<unsigned long long>(stats.total_frames_generated));
  ImGui::Text("Underruns: %llu  Overflows: %llu",
              static_cast<unsigned long long>(stats.underruns),
              static_cast<unsigned long long>(stats.overflows));

  // Generated rate estimate
  if (stats.total_frames_generated > 0) {
    ImGui::Text("Expected ratio: %.2f samples/sec",
                static_cast<double>(stats.sample_rate));
  }

  ImGui::Separator();

  // Mute toggles
  auto& apu = console.GetAPU();
  psg_muted_ = apu.IsPSGMuted();
  opm_muted_ = apu.IsOPMMuted();
  pcm_muted_ = apu.IsPCMMuted();

  if (ImGui::Checkbox("Mute PSG", &psg_muted_)) {
    apu.SetMutePSG(psg_muted_);
  }
  ImGui::SameLine();
  if (ImGui::Checkbox("Mute OPM", &opm_muted_)) {
    apu.SetMuteOPM(opm_muted_);
  }
  ImGui::SameLine();
  if (ImGui::Checkbox("Mute PCM", &pcm_muted_)) {
    apu.SetMutePCM(pcm_muted_);
  }

  ImGui::Separator();

  // Last writes
  sz::apu::APUDebugLastWrite writes[16];
  int write_count = 0;
  apu.GetLastWrites(writes, 16, write_count);

  if (write_count > 0) {
    ImGui::Text("Recent I/O Writes (%d):", write_count);
    for (int i = write_count - 1; i >= 0 && i >= write_count - 8; --i) {
      const auto& w = writes[i];
      const char* label = "???";
      if (w.port == 0x60) label = "PSG_DATA";
      else if (w.port == 0x70) label = "OPM_ADDR";
      else if (w.port == 0x71) label = "OPM_DATA";
      else if (w.port >= 0x72 && w.port <= 0x76) label = "PCM0";
      else if (w.port >= 0x77 && w.port <= 0x7B) label = "PCM1";
      else if (w.port == 0x7C) label = "MASTER_VOL";
      else if (w.port == 0x7D) label = "PAN";

      ImGui::Text("  [%llu] port=0x%02X (%s) val=0x%02X",
                  static_cast<unsigned long long>(w.cpu_cycle_timestamp),
                  w.port, label, w.value);
    }
  } else {
    ImGui::TextDisabled("No APU writes yet");
  }
}

}  // namespace sz::debugui
