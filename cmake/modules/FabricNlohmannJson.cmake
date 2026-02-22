# FabricNlohmannJson.cmake - Fetch and configure nlohmann/json
include(FetchContent)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    SYSTEM
    EXCLUDE_FROM_ALL
)

set(JSON_BuildTests OFF CACHE BOOL "Disable nlohmann/json tests" FORCE)
set(JSON_Install OFF CACHE BOOL "Disable nlohmann/json install" FORCE)
set(JSON_MultipleHeaders ON CACHE BOOL "Use multi-header layout" FORCE)

FetchContent_MakeAvailable(json)
