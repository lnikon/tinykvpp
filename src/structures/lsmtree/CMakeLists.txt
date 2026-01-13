cmake_minimum_required(VERSION 3.25)

add_library(
    LSMTree
    lsmtree.cpp
    segments/lsmtree_regular_segment.cpp
    segments/lsmtree_segment_factory.cpp
    segments/segment_storage.cpp
    segments/helpers.cpp
    levels/level.cpp
    levels/levels.cpp
)

set_target_properties(LSMTree PROPERTIES CXX_STANDARD 23)
target_include_directories(
    LSMTree
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
target_link_libraries(
    LSMTree
    PUBLIC
        spdlog::spdlog
        abseil::abseil
        libassert::assert
        magic_enum::magic_enum
        Microsoft.GSL::GSL
        Concurrency
        WAL
        Raft
        RaftProtoObjects
        Serialization
)

add_executable(LSMTreeTest lsmtree_test_basic.cpp)
set_target_properties(LSMTreeTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(
    LSMTreeTest
    PRIVATE
        gtest::gtest
        spdlog::spdlog
        fmt::fmt
        LSMTree
        MemTable
        Config
        HashIndex
        WAL
        Raft
        FS
        DB
        Concurrency
        Common
        Serialization
)

include(GoogleTest)
gtest_discover_tests(LSMTreeTest)
