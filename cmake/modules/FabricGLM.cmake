# FabricGLM.cmake - Fetch and configure GLM for Fabric Engine

CPMAddPackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG 1.0.3
    SYSTEM
    EXCLUDE_FROM_ALL
)
