# FabricNlohmannJson.cmake - Fetch and configure nlohmann/json

CPMAddPackage(
    NAME nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    OPTIONS
        "JSON_BuildTests OFF"
        "JSON_Install OFF"
        "JSON_MultipleHeaders ON"
    SYSTEM
    EXCLUDE_FROM_ALL
)
