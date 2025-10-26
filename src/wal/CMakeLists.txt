cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(WAL wal.cpp common.cpp)

set_target_properties(WAL PROPERTIES CXX_STANDARD 23)
target_include_directories(WAL PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(WAL PUBLIC abseil::abseil spdlog::spdlog FS RaftProtoObjects)

add_executable(WALTest wal_test.cpp)
set_target_properties(WALTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(WALTest GTest::gtest_main WAL)

include(GoogleTest)
gtest_discover_tests(WALTest)
