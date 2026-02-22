# FabricMimalloc.cmake - Fetch and configure mimalloc
include(FetchContent)

# mimalloc build options (set BEFORE FetchContent_MakeAvailable)
set(MI_BUILD_SHARED  OFF CACHE BOOL "Do not build mimalloc shared library"  FORCE)
set(MI_BUILD_STATIC  ON  CACHE BOOL "Build mimalloc static library"         FORCE)
set(MI_BUILD_OBJECT  OFF CACHE BOOL "Do not build mimalloc object library"  FORCE)
set(MI_BUILD_TESTS   OFF CACHE BOOL "Do not build mimalloc tests"           FORCE)
set(MI_OVERRIDE      ON  CACHE BOOL "Override standard malloc interface"    FORCE)
set(MI_SECURE        OFF CACHE BOOL "Security mitigations"                  FORCE)
set(MI_PADDING       OFF CACHE BOOL "Heap overflow padding"                 FORCE)
set(MI_INSTALL_TOPLEVEL OFF CACHE BOOL "Skip mimalloc install" FORCE)

FetchContent_Declare(
    mimalloc
    GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
    GIT_TAG        v2.2.7
    SYSTEM
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(mimalloc)
