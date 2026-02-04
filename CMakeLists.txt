cmake_minimum_required(VERSION 3.20)
project(SuperZ80 LANGUAGES CXX)

option(SUPERZ80_ENABLE_IMGUI "Enable Dear ImGui debug UI" ON)
option(SUPERZ80_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(SUPERZ80_ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Warnings.cmake)
include(cmake/Sanitizers.cmake)

find_package(SDL2 CONFIG QUIET)
if (SDL2_FOUND)
  message(STATUS "Using system SDL2 (set SDL2_DIR to override)")
else()
  message(STATUS "SDL2 not found; fetching SDL2 (set SDL2_DIR to use system package)")
  include(cmake/FetchSDL2.cmake)
  superz80_fetch_sdl2()
endif()

set(SUPERZ80_CORE_SOURCES
  src/console/SuperZ80Console.cpp
  src/core/log/Logger.cpp
  src/core/log/Trace.cpp
  src/cpu/Z80CpuStub.cpp
  src/devices/apu/APU.cpp
  src/devices/bus/Bus.cpp
  src/devices/cart/Cartridge.cpp
  src/devices/dma/DMAEngine.cpp
  src/devices/input/InputController.cpp
  src/devices/irq/IRQController.cpp
  src/devices/ppu/PPU.cpp
  src/devices/scheduler/Scheduler.cpp
)

add_library(superz80_core STATIC ${SUPERZ80_CORE_SOURCES})
target_include_directories(superz80_core PUBLIC src)
superz80_enable_warnings(superz80_core ${SUPERZ80_WARNINGS_AS_ERRORS})
if (SUPERZ80_ENABLE_SANITIZERS)
  superz80_enable_sanitizers(superz80_core)
endif()

set(SUPERZ80_APP_SOURCES
  src/main.cpp
  src/app/App.cpp
  src/app/InputHost.cpp
  src/app/SDLHost.cpp
  src/app/TimeSource.cpp
  src/app/VideoPresenter.cpp
)

if (SUPERZ80_ENABLE_IMGUI)
  add_compile_definitions(SUPERZ80_ENABLE_IMGUI)
  include(cmake/FetchImGui.cmake)
  superz80_fetch_imgui()

  list(APPEND SUPERZ80_APP_SOURCES
    src/debugui/DebugUI.cpp
    src/debugui/panels/PanelAPU.cpp
    src/debugui/panels/PanelBus.cpp
    src/debugui/panels/PanelCPU.cpp
    src/debugui/panels/PanelCartridge.cpp
    src/debugui/panels/PanelDMA.cpp
    src/debugui/panels/PanelIRQ.cpp
    src/debugui/panels/PanelInput.cpp
    src/debugui/panels/PanelPPU.cpp
    src/debugui/panels/PanelScheduler.cpp
  )
endif()

add_executable(superz80_app ${SUPERZ80_APP_SOURCES})
target_include_directories(superz80_app PRIVATE src)
target_link_libraries(superz80_app PRIVATE superz80_core)

if (TARGET SDL2::SDL2)
  target_link_libraries(superz80_app PRIVATE SDL2::SDL2)
elseif (TARGET SDL2::SDL2-static)
  target_link_libraries(superz80_app PRIVATE SDL2::SDL2-static)
endif()
if (TARGET SDL2::SDL2main)
  target_link_libraries(superz80_app PRIVATE SDL2::SDL2main)
endif()

if (SUPERZ80_ENABLE_IMGUI)
  target_link_libraries(superz80_app PRIVATE imgui)
  if (TARGET SDL2::SDL2)
    target_link_libraries(imgui PUBLIC SDL2::SDL2)
  elseif (TARGET SDL2::SDL2-static)
    target_link_libraries(imgui PUBLIC SDL2::SDL2-static)
  endif()
endif()

superz80_enable_warnings(superz80_app ${SUPERZ80_WARNINGS_AS_ERRORS})
if (SUPERZ80_ENABLE_SANITIZERS)
  superz80_enable_sanitizers(superz80_app)
endif()
