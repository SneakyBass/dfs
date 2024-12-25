cmake_minimum_required(VERSION 3.30)
project(dfsbot VERSION 1.0)

# Main project
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

file(GLOB_RECURSE SOURCE_LIST src/*.cc)
set(LIB_NAME dfsbot)

add_subdirectory(protocol)

if(ENABLE_TESTS)
    add_subdirectory(tests)
endif()

add_library(
    "${LIB_NAME}" SHARED
    "${SOURCE_LIST}"
)
target_link_libraries("${LIB_NAME}" fmt::fmt)

include_directories("${CMAKE_SOURCE_DIR}/include")
include_directories(SYSTEM "${CMAKE_CURRENT_LIST_DIR}/protocol")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/third-party")
