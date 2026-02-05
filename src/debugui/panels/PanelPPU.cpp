#include "debugui/panels/PanelPPU.h"

#include <imgui.h>

#include "devices/ppu/PPU.h"

namespace sz::debugui {

void PanelPPU::Draw(const sz::console::SuperZ80Console& console) {
  auto state = console.GetPPUDebugState();

  // Status section
  ImGui::Text("Phase 8: PPU Plane A + Palette RAM");
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

  // Palette viewer (Phase 8)
  if (ImGui::CollapsingHeader("Palette Viewer", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawPaletteViewer(console);
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
  const auto& ppu = console.GetPPU();
  const auto& active_rgb = ppu.GetActiveRgb888();

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

      // Phase 8: Get color from PPU's active palette
      u32 color = active_rgb[palette_index];

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

void PanelPPU::DrawPaletteViewer(const sz::console::SuperZ80Console& console) {
  auto state = console.GetPPUDebugState();
  const auto& ppu = console.GetPPU();

  // Phase 8: Palette I/O state
  ImGui::Text("PAL_ADDR: 0x%02X (entry=%d, byte=%s)",
              state.palette_debug.pal_addr,
              state.palette_debug.pal_index,
              state.palette_debug.pal_byte_sel == 0 ? "low" : "high");

  // Last write/commit tracking
  if (state.palette_debug.last_write_frame >= 0) {
    ImGui::Text("Last Write: frame %d, scanline %d, entry %d (%s byte)",
                state.palette_debug.last_write_frame,
                state.palette_debug.last_write_scanline,
                state.palette_debug.last_write_entry,
                state.palette_debug.last_write_byte_sel == 0 ? "low" : "high");
  } else {
    ImGui::Text("Last Write: none");
  }

  if (state.palette_debug.last_commit_frame >= 0) {
    ImGui::Text("Last Commit: frame %d, scanline %d",
                state.palette_debug.last_commit_frame,
                state.palette_debug.last_commit_scanline);
  } else {
    ImGui::Text("Last Commit: none");
  }

  ImGui::Separator();

  // Toggle: Active vs Staged palette view
  ImGui::Checkbox("Show Staged Palette", &palette_show_staged_);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(60);
  ImGui::InputInt("Swatch Scale", &palette_swatch_scale_, 1, 2);
  if (palette_swatch_scale_ < 1) palette_swatch_scale_ = 1;
  if (palette_swatch_scale_ > 8) palette_swatch_scale_ = 8;

  // Get the palette to display
  const auto& staged_pal = ppu.GetStagedPalette();
  const auto& active_pal = ppu.GetActivePalette();
  const auto& active_rgb = ppu.GetActiveRgb888();
  const auto& display_pal = palette_show_staged_ ? staged_pal : active_pal;

  ImGui::Separator();
  ImGui::Text("%s Palette (128 entries):", palette_show_staged_ ? "Staged" : "Active");

  // Draw palette grid (16 columns x 8 rows)
  ImVec2 start_pos = ImGui::GetCursorScreenPos();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  float swatch_size = static_cast<float>(palette_swatch_scale_ * 8);

  for (int i = 0; i < 128; ++i) {
    int col = i % 16;
    int row = i / 16;

    // Get color - expand if needed
    u32 argb;
    if (palette_show_staged_) {
      // Expand staged palette entry manually
      u16 packed = display_pal[i];
      int r3 = packed & 0x7;
      int g3 = (packed >> 3) & 0x7;
      int b3 = (packed >> 6) & 0x7;
      int r8 = (r3 * 255) / 7;
      int g8 = (g3 * 255) / 7;
      int b8 = (b3 * 255) / 7;
      argb = 0xFF000000u | (static_cast<u32>(r8) << 16) | (static_cast<u32>(g8) << 8) | static_cast<u32>(b8);
    } else {
      argb = active_rgb[i];
    }

    // Convert ARGB to ImGui color (ABGR)
    u32 imgui_color = (argb & 0xFF00FF00u) |               // A and G stay
                      ((argb & 0x00FF0000u) >> 16) |       // R -> B
                      ((argb & 0x000000FFu) << 16);        // B -> R

    float x1 = start_pos.x + static_cast<float>(col) * swatch_size;
    float y1 = start_pos.y + static_cast<float>(row) * swatch_size;
    float x2 = x1 + swatch_size;
    float y2 = y1 + swatch_size;

    draw_list->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), imgui_color);
    draw_list->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), 0xFF404040u);  // Grid border
  }

  // Reserve space for the palette grid
  float grid_width = 16.0f * swatch_size;
  float grid_height = 8.0f * swatch_size;
  ImGui::Dummy(ImVec2(grid_width, grid_height));

  // Show hover info
  ImVec2 mouse_pos = ImGui::GetMousePos();
  if (mouse_pos.x >= start_pos.x && mouse_pos.x < start_pos.x + grid_width &&
      mouse_pos.y >= start_pos.y && mouse_pos.y < start_pos.y + grid_height) {
    int col = static_cast<int>((mouse_pos.x - start_pos.x) / swatch_size);
    int row = static_cast<int>((mouse_pos.y - start_pos.y) / swatch_size);
    int entry = row * 16 + col;
    if (entry < 128) {
      u16 packed = display_pal[entry];
      int r3 = packed & 0x7;
      int g3 = (packed >> 3) & 0x7;
      int b3 = (packed >> 6) & 0x7;
      ImGui::Text("Entry %d: packed=0x%03X  R=%d G=%d B=%d", entry, packed, r3, g3, b3);
    }
  }
}

}  // namespace sz::debugui
