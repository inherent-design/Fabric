# FabricTracy.cmake - Fetch and configure Tracy Profiler

option(FABRIC_ENABLE_PROFILING "Enable Tracy profiler instrumentation" OFF)

if(FABRIC_ENABLE_PROFILING)
    CPMAddPackage(
        NAME tracy
        GITHUB_REPOSITORY wolfpld/tracy
        GIT_TAG v0.13.1
        OPTIONS
            "TRACY_ENABLE ON"
            "TRACY_ON_DEMAND ON"
            "TRACY_NO_EXIT OFF"
        SYSTEM
        EXCLUDE_FROM_ALL
    )
endif()

# Convenience function: link Tracy to a target (no-op when profiling is disabled)
function(fabric_link_tracy TARGET)
    if(FABRIC_ENABLE_PROFILING)
        target_link_libraries(${TARGET} PUBLIC Tracy::TracyClient)
        target_compile_definitions(${TARGET} PUBLIC FABRIC_PROFILING_ENABLED)
    endif()
endfunction()
