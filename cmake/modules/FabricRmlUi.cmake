# RmlUi - HTML/CSS UI library with vertex-level rendering control
# Scaffold only: FetchContent + find target. Full bgfx RenderInterface is Sprint 3.
include_guard()

include(FetchContent)

FetchContent_Declare(
    RmlUi
    GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
    GIT_TAG        6.0
    GIT_SHALLOW    TRUE
    SYSTEM
    EXCLUDE_FROM_ALL
)

# Disable samples and tests
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_TESTS OFF CACHE BOOL "" FORCE)
set(RMLUI_THIRDPARTY_CONTAINERS ON CACHE BOOL "" FORCE)

# FreeType font engine for text rendering (requires system FreeType)
set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "" FORCE)

# Preserve BUILD_SHARED_LIBS state: RmlUi may flip it to ON
set(_FABRIC_SAVED_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)

FetchContent_MakeAvailable(RmlUi)

# Restore BUILD_SHARED_LIBS
set(BUILD_SHARED_LIBS ${_FABRIC_SAVED_BUILD_SHARED_LIBS})
