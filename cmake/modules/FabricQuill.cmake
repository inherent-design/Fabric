# FabricQuill.cmake - Fetch and configure Quill logging

CPMAddPackage(
    NAME quill
    GITHUB_REPOSITORY odygrd/quill
    GIT_TAG v11.0.2
    SYSTEM
    EXCLUDE_FROM_ALL
)
