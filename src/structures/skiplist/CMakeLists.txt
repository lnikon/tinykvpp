cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(SkipList INTERFACE)
target_sources(
    SkipList
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/structures/skiplist/skiplist.h
)

set_target_properties(SkipList PROPERTIES CXX_STANDARD 23)
target_include_directories(
    SkipList
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

add_executable(SkipListTest skiplist_test.cpp)
set_target_properties(SkipListTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(SkipListTest PRIVATE gtest::gtest SkipList)

include(GoogleTest)
gtest_discover_tests(SkipListTest)
