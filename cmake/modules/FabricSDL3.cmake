# FabricSDL3.cmake - Fetch and configure SDL3
include_guard()

set(SDL_SHARED OFF)
set(SDL_STATIC ON)
set(SDL_TEST OFF)

# Enable core SDL3 subsystems
foreach(SUBSYSTEM IN ITEMS AUDIO VIDEO RENDER EVENTS JOYSTICK HIDAPI SENSOR THREADS TIMERS)
    set(SDL_${SUBSYSTEM} ON)
endforeach()

CPMAddPackage(
    NAME SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.2
    SYSTEM
    EXCLUDE_FROM_ALL
)
