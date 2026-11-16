cmake_minimum_required(VERSION 3.20)
project(ArenaFighters VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# raylib 5.0 declares cmake_minimum_required(< 3.5), which CMake 4.x rejects without this.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# ---------------------------------------------------------------------------
# Third-party dependencies (downloaded automatically on first configure)
# ---------------------------------------------------------------------------

# raylib - 2D rendering, window and keyboard input
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# SQLite amalgamation - embedded relational database
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sqlite3)

find_package(Threads REQUIRED)
add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
target_link_libraries(sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

# ---------------------------------------------------------------------------
# Project targets
# ---------------------------------------------------------------------------

# Persistence layer has no graphics dependency, so it can be unit-tested headless.
add_library(arena_core STATIC src/Database.cpp)
target_include_directories(arena_core PUBLIC src)
target_link_libraries(arena_core PUBLIC sqlite3)

add_executable(arena_fighters
    src/main.cpp
    src/Game.cpp
    src/Match.cpp
    src/Fighter.cpp
    src/Characters.cpp
    src/Ranking.cpp
    src/UI.cpp
)
target_link_libraries(arena_fighters PRIVATE arena_core raylib)

# Lets the game find db/ and ranking/ when launched from the build directory.
# Can be overridden at runtime with the ARENA_ROOT environment variable.
target_compile_definitions(arena_fighters PRIVATE ARENA_DEFAULT_ROOT="${CMAKE_SOURCE_DIR}")

if(MSVC)
    target_compile_options(arena_fighters PRIVATE /W4)
else()
    target_compile_options(arena_fighters PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(test_database tests/test_database.cpp)
target_link_libraries(test_database PRIVATE arena_core)
target_compile_definitions(test_database PRIVATE ARENA_DEFAULT_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME database COMMAND test_database)
