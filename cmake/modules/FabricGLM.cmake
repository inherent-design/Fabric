# FabricGLM.cmake - Fetch and configure GLM for Fabric Engine
include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
    SYSTEM
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(glm)
