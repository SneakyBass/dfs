cmake_minimum_required(VERSION 3.30)
project(dfs VERSION 1.0)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    add_definitions(-DNDEBUG)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -Wextra")

file(GLOB_RECURSE SOURCE_LIST src/*.cc)
set(EXECUTABLE_NAME "dfs")

# Options
option(ENABLE_ASAN "Compile ASAN" "OFF")
option(ENABLE_TESTS "Build tests" "ON")

if(ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

if(ENABLE_TESTS)
    set(GTEST_COMMIT_HASH "b514bdc898e2951020cbdca1304b75f5950d1f59")

    include(FetchContent)
    fetchcontent_declare(
        googletest
        URL
        https://github.com/google/googletest/archive/${GTEST_COMMIT_HASH}.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    fetchcontent_makeavailable(googletest)

    enable_testing()
endif()

# Dependencies check
find_package(fmt REQUIRED)

# Add the bot
add_subdirectory(bot)

# Executable
add_executable("${EXECUTABLE_NAME}" "${SOURCE_LIST}")
target_link_libraries("${EXECUTABLE_NAME}" dfsbot protocol fmt::fmt)

# Workaround for Abseil (this fucking library again...)
set_target_properties("${EXECUTABLE_NAME}" PROPERTIES
    LINK_FLAGS "-Wl,--copy-dt-needed-entries"
)

include_directories("${CMAKE_SOURCE_DIR}/include")
