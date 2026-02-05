#ifndef SUPERZ80_DEBUGUI_PANELS_PANELPPU_H
#define SUPERZ80_DEBUGUI_PANELS_PANELPPU_H

#include "console/SuperZ80Console.h"
#include "core/types.h"

namespace sz::debugui {

class PanelPPU {
 public:
  void Draw(const sz::console::SuperZ80Console& console);

 private:
  void DrawRegistersPanel(const sz::ppu::DebugState& state);
  void DrawVramViewer(const sz::console::SuperZ80Console& console);
  void DrawTileViewer(const sz::console::SuperZ80Console& console);

  // VRAM viewer state
  int vram_view_address_ = 0;
  int vram_view_rows_ = 16;

  // Tile viewer state
  int tile_viewer_index_ = 0;
  int tile_viewer_scale_ = 8;
};

}  // namespace sz::debugui

#endif
