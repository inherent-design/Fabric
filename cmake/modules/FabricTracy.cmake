# FabricTracy.cmake - Fetch and configure Tracy Profiler
include(FetchContent)

option(FABRIC_ENABLE_PROFILING "Enable Tracy profiler instrumentation" OFF)

if(FABRIC_ENABLE_PROFILING)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG        v0.13.1
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
        SYSTEM
        EXCLUDE_FROM_ALL
    )

    set(TRACY_ENABLE    ON  CACHE BOOL "Enable Tracy profiling"       FORCE)
    set(TRACY_ON_DEMAND ON  CACHE BOOL "Profile only when connected"  FORCE)
    set(TRACY_NO_EXIT   OFF CACHE BOOL "Wait for profiler on exit"    FORCE)

    FetchContent_MakeAvailable(tracy)
endif()

# Convenience function: link Tracy to a target (no-op when profiling is disabled)
function(fabric_link_tracy TARGET)
    if(FABRIC_ENABLE_PROFILING)
        target_link_libraries(${TARGET} PUBLIC Tracy::TracyClient)
        target_compile_definitions(${TARGET} PUBLIC FABRIC_PROFILING_ENABLED)
    endif()
endfunction()
