cmake_minimum_required(VERSION 3.30)
project(proto VERSION 1.0)

file(GLOB_RECURSE PROTO_CONNECTION_LIST connection/*.cc)
file(GLOB_RECURSE PROTO_GAME_LIST game/*.cc)
set(PROTO_LIB protocol)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)
find_package(absl REQUIRED)
find_program(FOUND_PROTOC protoc REQUIRED)

add_library(
    "${PROTO_LIB}" STATIC
    "${PROTO_CONNECTION_LIST}"
    "${PROTO_GAME_LIST}"
)
target_link_libraries(
    ${PROTO_LIB} PUBLIC
    "${Protobuf_LIBRARIES}"
    "${absl_LIBRARIES}"
)

include_directories(SYSTEM "${Protobuf_INCLUDE_DIRS}")
