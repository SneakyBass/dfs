cmake_minimum_required(VERSION 3.30)
project(proto VERSION 1.0)

file(GLOB_RECURSE TEST_FILES src/*.cc)

add_executable(
    tests
    "${TEST_FILES}"
)

target_link_libraries(
    tests
    dfsbot
    protocol
    GTest::gtest_main
)

include(GoogleTest)

include_directories("${CMAKE_SOURCE_DIR}/include")

set_target_properties(tests PROPERTIES
    LINK_FLAGS "-Wl,--copy-dt-needed-entries"
)

gtest_discover_tests(tests)
