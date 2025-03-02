cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(
        SkipList
        "skiplist.cpp")

set_target_properties(SkipList PROPERTIES CXX_STANDARD 23)
target_include_directories(SkipList INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(SkipListTest "skiplist_test.cpp")
set_target_properties(SkipListTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(
        SkipListTest
        Catch2::Catch2WithMain
        spdlog::spdlog
        fmt::fmt
        LSMTree
        SkipList
        MemTable
        Config
)

# Register Catch2 tests with CTest
include(Catch)
catch_discover_tests(SkipListTest)
