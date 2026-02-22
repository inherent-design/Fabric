# FabricGoogleTest.cmake - Fetch and configure Google Test
include(FetchContent)

# Prevent installation of Google Test (must be set before FetchContent_MakeAvailable)
set(INSTALL_GTEST OFF CACHE BOOL "Disable Google Test installation" FORCE)

# Download and configure Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    SYSTEM
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(googletest)

# GoogleTest's CMake targets (GTest::gtest, etc.) already export proper include directories.
# No manual include_directories() needed.

# Function to add a test executable
function(add_fabric_test TEST_NAME TEST_SOURCES)
    add_executable(${TEST_NAME} ${TEST_SOURCES})

    target_link_libraries(${TEST_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
        FabricLib
    )

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    set_tests_properties(${TEST_NAME} PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endfunction()
