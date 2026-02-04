#include "debugui/DebugUI.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "debugui/panels/PanelAPU.h"
#include "debugui/panels/PanelBus.h"
#include "debugui/panels/PanelCPU.h"
#include "debugui/panels/PanelCartridge.h"
#include "debugui/panels/PanelDMA.h"
#include "debugui/panels/PanelIRQ.h"
#include "debugui/panels/PanelInput.h"
#include "debugui/panels/PanelPPU.h"
#include "debugui/panels/PanelScheduler.h"

namespace sz::debugui {

void DebugUI::Init(SDL_Window* window, SDL_Renderer* renderer) {
  if (initialized_) {
    return;
  }
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer2_Init(renderer);
  initialized_ = true;
}

void DebugUI::Shutdown() {
  if (!initialized_) {
    return;
  }
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  initialized_ = false;
}

void DebugUI::ProcessEvent(const SDL_Event* event) {
  if (!initialized_) {
    return;
  }
  ImGui_ImplSDL2_ProcessEvent(event);
}

void DebugUI::BeginFrame() {
  if (!initialized_) {
    return;
  }
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
}

void DebugUI::Draw(const sz::console::SuperZ80Console& console) {
  if (!initialized_) {
    return;
  }

  ImGui::Begin("SuperZ80 Debug");

  PanelCPU cpu_panel;
  PanelBus bus_panel;
  PanelPPU ppu_panel;
  PanelAPU apu_panel;
  PanelDMA dma_panel;
  PanelIRQ irq_panel;
  PanelScheduler scheduler_panel;
  PanelCartridge cart_panel;
  PanelInput input_panel;

  if (ImGui::CollapsingHeader("CPU", ImGuiTreeNodeFlags_DefaultOpen)) {
    cpu_panel.Draw(console);
  }
  if (ImGui::CollapsingHeader("Bus", ImGuiTreeNodeFlags_DefaultOpen)) {
    bus_panel.Draw(console);
  }
  if (ImGui::CollapsingHeader("PPU", ImGuiTreeNodeFlags_DefaultOpen)) {
    ppu_panel.Draw(console);
  }
  if (ImGui::CollapsingHeader("APU", ImGuiTreeNodeFlags_DefaultOpen)) {
    apu_panel.Draw(console);
  }
  if (ImGui::CollapsingHeader("DMA", ImGuiTreeNodeFlags_DefaultOpen)) {
    dma_panel.Draw(console);
  }
  if (ImGui::CollapsingHeader("IRQ", ImGuiTreeNodeFlags_DefaultOpen)) {
    irq_panel.Draw(console);
  }
  if (ImGui::CollapsingHeader("Scheduler/Timing", ImGuiTreeNodeFlags_DefaultOpen)) {
    scheduler_panel.Draw(console);
  }
  if (ImGui::CollapsingHeader("Cartridge", ImGuiTreeNodeFlags_DefaultOpen)) {
    cart_panel.Draw(console);
  }
  if (ImGui::CollapsingHeader("Input", ImGuiTreeNodeFlags_DefaultOpen)) {
    input_panel.Draw(console);
  }

  ImGui::End();
}

void DebugUI::EndFrame(SDL_Renderer* renderer) {
  if (!initialized_) {
    return;
  }
  ImGui::Render();
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
}

}  // namespace sz::debugui
