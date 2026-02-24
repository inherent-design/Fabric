# FabricAsio.cmake - Fetch and configure Standalone Asio (header-only)

CPMAddPackage(
    NAME asio
    GITHUB_REPOSITORY chriskohlhoff/asio
    GIT_TAG asio-1-36-0
    SYSTEM
    EXCLUDE_FROM_ALL
)

# Create INTERFACE library target (header-only, no compilation)
add_library(asio::asio INTERFACE IMPORTED GLOBAL)

# Asio headers live under asio/include/ in the repo
target_include_directories(asio::asio INTERFACE
    ${asio_SOURCE_DIR}/asio/include
)

# Core defines for standalone + C++20 coroutine support
target_compile_definitions(asio::asio INTERFACE
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
    ASIO_HAS_CO_AWAIT
    ASIO_HAS_STD_COROUTINE
)
