# FabricMiniaudio.cmake - Fetch miniaudio (single-header audio library)
include_guard()

CPMAddPackage(
    NAME miniaudio
    GITHUB_REPOSITORY mackron/miniaudio
    GIT_TAG 0.11.22
    DOWNLOAD_ONLY YES
)

if(miniaudio_ADDED)
    add_library(miniaudio INTERFACE)
    target_include_directories(miniaudio SYSTEM INTERFACE ${miniaudio_SOURCE_DIR})
endif()
