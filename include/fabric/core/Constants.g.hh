#pragma once

namespace fabric {
constexpr const char* APP_NAME = "Fabric";
#if defined(_WIN32)
constexpr const char* APP_EXECUTABLE_NAME = "Fabric.exe";
#else
constexpr const char* APP_EXECUTABLE_NAME = "Fabric";
#endif
constexpr const char* APP_VERSION = "0.1.0";
} // namespace fabric
