cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(SkipList skiplist.cpp)

set_target_properties(SkipList PROPERTIES CXX_STANDARD 23)
target_include_directories(
    SkipList
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

add_executable(SkipListTest skiplist_test.cpp)
set_target_properties(SkipListTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(
    SkipListTest
    gtest::gtest
    spdlog::spdlog
    fmt::fmt
    LSMTree
    SkipList
    MemTable
    Config
)

# include(GoogleTest)
# gtest_discover_tests(SkipListTest)
