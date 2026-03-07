# FabricWebView.cmake - Fetch and configure WebView
include_guard()

option(FABRIC_USE_WEBVIEW "Enable WebView support" ON)

# Set WebKitGTK API version for Linux (4.1 is available on Ubuntu 22.04+, Fedora 38+, Arch)
if(UNIX AND NOT APPLE)
    set(WEBVIEW_WEBKITGTK_API "4.1" CACHE STRING "WebKitGTK API version")
endif()

CPMAddPackage(
    NAME webview
    GITHUB_REPOSITORY webview/webview
    GIT_TAG 0.12.0
    SYSTEM
    EXCLUDE_FROM_ALL
)
