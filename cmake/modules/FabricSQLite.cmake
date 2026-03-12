# FabricSQLite.cmake - Fetch and configure SQLite3 amalgamation
include_guard()

CPMAddPackage(
    NAME sqlite3
    VERSION 3.49.1
    DOWNLOAD_ONLY TRUE
    URL "https://www.sqlite.org/2025/sqlite-amalgamation-3490100.zip"
)

if(sqlite3_ADDED)
    add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
    target_compile_definitions(sqlite3 PRIVATE
        SQLITE_THREADSAFE=2
        SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
        SQLITE_DEFAULT_JOURNAL_MODE=WAL
        SQLITE_OMIT_LOAD_EXTENSION
        SQLITE_ENABLE_RTREE
        SQLITE_DQS=0
    )
    if(NOT APPLE)
        find_package(Threads REQUIRED)
        target_link_libraries(sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
    endif()
endif()
