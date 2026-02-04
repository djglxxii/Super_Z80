include(FetchContent)

function(superz80_fetch_sdl2)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    sdl2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.2
  )
  FetchContent_MakeAvailable(sdl2)

  if (TARGET SDL2::SDL2-static AND NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 ALIAS SDL2::SDL2-static)
  endif()
endfunction()
