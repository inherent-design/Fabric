# FabricMimalloc.cmake - Fetch and configure mimalloc

CPMAddPackage(
    NAME mimalloc
    GITHUB_REPOSITORY microsoft/mimalloc
    GIT_TAG v2.2.7
    OPTIONS
        "MI_BUILD_SHARED OFF"
        "MI_BUILD_STATIC ON"
        "MI_BUILD_OBJECT OFF"
        "MI_BUILD_TESTS OFF"
        "MI_OVERRIDE ON"
        "MI_SECURE OFF"
        "MI_PADDING OFF"
        "MI_INSTALL_TOPLEVEL OFF"
    SYSTEM
    EXCLUDE_FROM_ALL
)
