cmake_minimum_required(VERSION 3.27)
project(zkv)

add_library(
        LSMTree
        "lsmtree.cpp"
        "segments/segment_interface.cpp"
        "segments/lsmtree_mock_segment.cpp"
        "segments/lsmtree_regular_segment.cpp"
        "segments/lsmtree_segment_factory.cpp"
        "segments/segment_storage.cpp"
        "segments/helpers.cpp"
        "segments/types.h"
        "levels/levels.cpp"
        "levels/level_zero.cpp"
        "levels/level_non_zero.cpp"
        "compactation/level_zero_compactation.cpp")

set_target_properties(LSMTree PROPERTIES CXX_STANDARD 23)
target_include_directories(LSMTree INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LSMTree PUBLIC Boost::iostreams)

add_executable(LSMTreeTest "lsmtree_test.cpp")
set_target_properties(LSMTreeTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(
        LSMTreeTest
        PRIVATE
        Catch2::Catch2WithMain
        gRPC::grpc
        spdlog::spdlog
        fmt::fmt
        ZLIB::ZLIB
        LSMTree
        MemTable
        Config
        HashIndex)
