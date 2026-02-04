# Phase 0 Done (Checklist Mapping)

PHASE 0 DONE when all are true:

1. Repository structure matches architecture document:
   - SuperZ80Console exists and owns: Scheduler, Bus, IRQController, Cartridge, PPU, APU, DMAEngine, InputController (as members).
   - CPU wrapper exists under src/cpu but is not integrated into execution/bus wiring. (Scaffold only.)
2. CMake builds a runnable executable on at least Windows + Linux + macOS (no platform-specific hacks required).
3. SDL2 window opens at internal resolution 256×192 presented with integer scaling (default scale > 1 is fine).
4. Main loop exists; performs no emulation (no Z80 execution, no register behaviors). It only steps the scaffolding tick + presents a test pattern framebuffer.
5. Logging + assertions are in place and used at least once during init (e.g., log build info; assert framebuffer size).
6. Debug UI shell loads (if enabled) and shows empty panels for CPU/Bus/PPU/APU/DMA/IRQ/Scheduler-Cartridge-Input with placeholder text.
7. Pass criteria: executable runs, opens window, exits cleanly via close button or Esc key without leaks/crash.
