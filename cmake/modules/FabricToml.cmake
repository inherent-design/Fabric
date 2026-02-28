# FabricToml.cmake - Fetch and configure toml++ (header-only TOML v1.0 parser)

CPMAddPackage(
    NAME tomlplusplus
    GITHUB_REPOSITORY marzer/tomlplusplus
    VERSION 3.4.0
    OPTIONS
        "TOMLPLUSPLUS_INSTALL OFF"
    SYSTEM
    EXCLUDE_FROM_ALL
)
