cmake_minimum_required(VERSION 3.25)

add_library(
        LSMTree
        "lsmtree.cpp"
        "segments/lsmtree_regular_segment.cpp"
        "segments/lsmtree_segment_factory.cpp"
        "segments/segment_storage.cpp"
        "segments/uuid.h"
        "segments/helpers.cpp"
        "segments/types.h"
        "levels/levels.cpp"
        "levels/level.cpp"
        "compactation/level_zero_compactation.cpp")

set_target_properties(LSMTree PROPERTIES CXX_STANDARD 23)
target_include_directories(LSMTree INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/segments)
target_link_libraries(LSMTree PRIVATE spdlog::spdlog)

add_executable(LSMTreeTest "lsmtree_test.cpp")
set_target_properties(LSMTreeTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(
        LSMTreeTest
        PRIVATE
        Catch2::Catch2WithMain
        spdlog::spdlog
        fmt::fmt
        LSMTree
        MemTable
        Config
        HashIndex
        FS
        DB)
