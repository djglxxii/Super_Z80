#include "debugui/panels/PanelPPU.h"

#include <array>
#include <cstdio>

#include <imgui.h>

#include "devices/ppu/PPU.h"

namespace sz::debugui {

namespace {
// Fixed palette for tile viewer (matches PPU's fixed palette)
constexpr std::array<u32, 16> kDebugPalette = {{
    0xFF000000u,  // 0: black
    0xFF0000AAu,  // 1: dark blue
    0xFF00AA00u,  // 2: dark green
    0xFF00AAAAu,  // 3: dark cyan
    0xFFAA0000u,  // 4: dark red
    0xFFAA00AAu,  // 5: dark magenta
    0xFFAA5500u,  // 6: brown
    0xFFAAAAAAu,  // 7: light gray
    0xFF555555u,  // 8: dark gray
    0xFF5555FFu,  // 9: blue
    0xFF55FF55u,  // 10: green
    0xFF55FFFFu,  // 11: cyan
    0xFFFF5555u,  // 12: red
    0xFFFF55FFu,  // 13: magenta
    0xFFFFFF55u,  // 14: yellow
    0xFFFFFFFFu,  // 15: white
}};
}  // namespace

void PanelPPU::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetPPUDebugState();

  // Status section
  ImGui::Text("Phase 7: PPU Plane A Renderer");
  ImGui::Separator();

  // VBlank status
  ImGui::Text("Scanline: %d  VBlank: %s", state.last_scanline,
              state.vblank_flag ? "TRUE" : "FALSE");
  ImGui::Text("VBlank latch count: %llu",
              static_cast<unsigned long long>(state.vblank_latch_count));
  ImGui::Separator();

  // Registers panel
  if (ImGui::CollapsingHeader("PPU Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawRegistersPanel(state);
  }

  // VRAM viewer
  if (ImGui::CollapsingHeader("VRAM Viewer")) {
    DrawVramViewer(console);
  }

  // Tile viewer
  if (ImGui::CollapsingHeader("Tile Viewer")) {
    DrawTileViewer(console);
  }
}

void PanelPPU::DrawRegistersPanel(const sz::ppu::DebugState& state) {
  ImGui::Columns(3, "regcols");
  ImGui::SetColumnWidth(0, 120);
  ImGui::SetColumnWidth(1, 80);
  ImGui::SetColumnWidth(2, 80);

  ImGui::Text("Register");
  ImGui::NextColumn();
  ImGui::Text("Pending");
  ImGui::NextColumn();
  ImGui::Text("Active");
  ImGui::NextColumn();
  ImGui::Separator();

  // VDP_CTRL (0x11)
  ImGui::Text("VDP_CTRL (0x11)");
  ImGui::NextColumn();
  ImGui::Text("0x%02X", state.pending_regs.vdp_ctrl);
  ImGui::NextColumn();
  ImGui::Text("0x%02X", state.active_regs.vdp_ctrl);
  ImGui::NextColumn();

  // Display enable decoded
  ImGui::Text("  Display En");
  ImGui::NextColumn();
  ImGui::Text("%s", (state.pending_regs.vdp_ctrl & 0x01) ? "ON" : "OFF");
  ImGui::NextColumn();
  ImGui::Text("%s", (state.active_regs.vdp_ctrl & 0x01) ? "ON" : "OFF");
  ImGui::NextColumn();

  // Scroll X (0x12)
  ImGui::Text("Scroll X (0x12)");
  ImGui::NextColumn();
  ImGui::Text("%d", state.pending_regs.scroll_x);
  ImGui::NextColumn();
  ImGui::Text("%d", state.active_regs.scroll_x);
  ImGui::NextColumn();

  // Scroll Y (0x13)
  ImGui::Text("Scroll Y (0x13)");
  ImGui::NextColumn();
  ImGui::Text("%d", state.pending_regs.scroll_y);
  ImGui::NextColumn();
  ImGui::Text("%d", state.active_regs.scroll_y);
  ImGui::NextColumn();

  // Plane A Base (0x16)
  ImGui::Text("PlaneA Base (0x16)");
  ImGui::NextColumn();
  ImGui::Text("%d (0x%04X)", state.pending_regs.plane_a_base,
              state.pending_regs.plane_a_base * 1024);
  ImGui::NextColumn();
  ImGui::Text("%d (0x%04X)", state.active_regs.plane_a_base,
              state.active_regs.plane_a_base * 1024);
  ImGui::NextColumn();

  // Pattern Base (0x18)
  ImGui::Text("Pattern Base (0x18)");
  ImGui::NextColumn();
  ImGui::Text("%d (0x%04X)", state.pending_regs.pattern_base,
              state.pending_regs.pattern_base * 1024);
  ImGui::NextColumn();
  ImGui::Text("%d (0x%04X)", state.active_regs.pattern_base,
              state.active_regs.pattern_base * 1024);
  ImGui::NextColumn();

  ImGui::Columns(1);
}

void PanelPPU::DrawVramViewer(const sz::console::SuperZ80Console& console) {
  auto state = console.GetPPUDebugState();

  // Quick jump buttons
  if (ImGui::Button("Pattern Base")) {
    vram_view_address_ = state.active_regs.pattern_base * 1024;
  }
  ImGui::SameLine();
  if (ImGui::Button("Tilemap Base")) {
    vram_view_address_ = state.active_regs.plane_a_base * 1024;
  }
  ImGui::SameLine();
  if (ImGui::Button("Start")) {
    vram_view_address_ = 0;
  }

  // Address input
  ImGui::SetNextItemWidth(100);
  ImGui::InputInt("Address", &vram_view_address_, 16, 256);
  if (vram_view_address_ < 0) vram_view_address_ = 0;
  if (vram_view_address_ >= static_cast<int>(sz::ppu::PPU::kVramSizeBytes)) {
    vram_view_address_ = static_cast<int>(sz::ppu::PPU::kVramSizeBytes) - 16;
  }

  // Rows to display
  ImGui::SameLine();
  ImGui::SetNextItemWidth(60);
  ImGui::InputInt("Rows", &vram_view_rows_, 1, 4);
  if (vram_view_rows_ < 1) vram_view_rows_ = 1;
  if (vram_view_rows_ > 32) vram_view_rows_ = 32;

  // Read VRAM window
  size_t bytes_to_read = static_cast<size_t>(vram_view_rows_ * 16);
  auto vram = console.GetPPUVramWindow(static_cast<u16>(vram_view_address_), bytes_to_read);

  // Display hex dump
  ImGui::BeginChild("VramHex", ImVec2(0, 0), true);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

  for (int row = 0; row < vram_view_rows_ && row * 16 < static_cast<int>(vram.size()); ++row) {
    int addr = vram_view_address_ + row * 16;
    ImGui::Text("%04X: ", addr);
    ImGui::SameLine();

    for (int col = 0; col < 16 && row * 16 + col < static_cast<int>(vram.size()); ++col) {
      if (col == 8) {
        ImGui::SameLine();
        ImGui::Text(" ");
      }
      ImGui::SameLine();
      ImGui::Text("%02X ", vram[static_cast<size_t>(row * 16 + col)]);
    }
  }

  ImGui::PopStyleVar();
  ImGui::EndChild();
}

void PanelPPU::DrawTileViewer(const sz::console::SuperZ80Console& console) {
  auto state = console.GetPPUDebugState();

  // Tile index input
  ImGui::SetNextItemWidth(100);
  ImGui::InputInt("Tile Index", &tile_viewer_index_, 1, 16);
  if (tile_viewer_index_ < 0) tile_viewer_index_ = 0;
  if (tile_viewer_index_ > 1023) tile_viewer_index_ = 1023;

  ImGui::SameLine();
  ImGui::SetNextItemWidth(60);
  ImGui::InputInt("Scale", &tile_viewer_scale_, 1, 2);
  if (tile_viewer_scale_ < 1) tile_viewer_scale_ = 1;
  if (tile_viewer_scale_ > 16) tile_viewer_scale_ = 16;

  // Calculate tile address
  u32 pattern_base = static_cast<u32>(state.active_regs.pattern_base) * 1024;
  u32 tile_addr = pattern_base + static_cast<u32>(tile_viewer_index_) * 32;
  ImGui::Text("Tile %d at VRAM 0x%04X (pattern base 0x%04X)", tile_viewer_index_,
              tile_addr, pattern_base);

  // Read tile data (32 bytes)
  auto tile_data = console.GetPPUVramWindow(static_cast<u16>(tile_addr), 32);
  if (tile_data.size() < 32) {
    ImGui::Text("Error: Could not read tile data");
    return;
  }

  // Draw the tile as colored rectangles
  ImVec2 start_pos = ImGui::GetCursorScreenPos();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  float pixel_size = static_cast<float>(tile_viewer_scale_);

  for (int y = 0; y < 8; ++y) {
    int row_offset = y * 4;
    for (int x = 0; x < 8; ++x) {
      int byte_offset = x / 2;
      u8 byte_val = tile_data[static_cast<size_t>(row_offset + byte_offset)];

      // Extract nibble: high nibble for even X, low nibble for odd X
      u8 palette_index;
      if ((x & 1) == 0) {
        palette_index = (byte_val >> 4) & 0x0F;
      } else {
        palette_index = byte_val & 0x0F;
      }

      // Get color from palette
      u32 color = kDebugPalette[palette_index];

      // Convert ARGB to ImGui color (ABGR)
      u32 imgui_color = (color & 0xFF00FF00u) |               // A and G stay
                        ((color & 0x00FF0000u) >> 16) |       // R -> B
                        ((color & 0x000000FFu) << 16);        // B -> R

      float x1 = start_pos.x + static_cast<float>(x) * pixel_size;
      float y1 = start_pos.y + static_cast<float>(y) * pixel_size;
      float x2 = x1 + pixel_size;
      float y2 = y1 + pixel_size;

      draw_list->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), imgui_color);
    }
  }

  // Reserve space for the tile display
  float total_size = 8.0f * pixel_size;
  ImGui::Dummy(ImVec2(total_size, total_size));

  // Show hex dump of tile data
  ImGui::Separator();
  ImGui::Text("Tile Data (32 bytes, 4bpp packed):");
  for (int row = 0; row < 8; ++row) {
    ImGui::Text("Row %d:", row);
    ImGui::SameLine();
    for (int b = 0; b < 4; ++b) {
      ImGui::SameLine();
      ImGui::Text("%02X", tile_data[static_cast<size_t>(row * 4 + b)]);
    }
  }
}

}  // namespace sz::debugui
