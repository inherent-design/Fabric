# FabricEnkiTS.cmake - Fetch and configure enkiTS task scheduler
include_guard()

# enkiTS v1.11 declares cmake_minimum_required(VERSION 3.0), which CMake 4.x
# rejects. Allow the legacy policy version for this dependency only.
set(_FABRIC_SAVED_POLICY_MIN "${CMAKE_POLICY_VERSION_MINIMUM}")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

CPMAddPackage(
    NAME enkiTS
    GITHUB_REPOSITORY dougbinks/enkiTS
    GIT_TAG v1.11
    OPTIONS
        "ENKITS_BUILD_EXAMPLES OFF"
        "ENKITS_BUILD_C_INTERFACE OFF"
        "ENKITS_TASK_PRIORITIES_NUM 3"
    SYSTEM
    EXCLUDE_FROM_ALL
)

set(CMAKE_POLICY_VERSION_MINIMUM "${_FABRIC_SAVED_POLICY_MIN}")

# enkiTS uses legacy include_directories() which does not propagate to
# consumers through target linkage. Expose the src/ directory so that
# #include <TaskScheduler.h> resolves for any target linking enkiTS.
if(enkiTS_ADDED)
  target_include_directories(enkiTS SYSTEM PUBLIC "${enkiTS_SOURCE_DIR}/src")
endif()
