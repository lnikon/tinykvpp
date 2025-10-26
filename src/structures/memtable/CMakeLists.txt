cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(MemTable memtable.cpp)
target_compile_features(MemTable PUBLIC cxx_std_23)
target_include_directories(
    MemTable
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
target_link_libraries(MemTable PRIVATE fmt::fmt)

add_executable(MemTableTest memtable_test.cpp)
target_compile_features(MemTableTest PUBLIC cxx_std_23)
target_link_libraries(
    MemTableTest
    PRIVATE Catch2::Catch2WithMain spdlog::spdlog fmt::fmt LSMTree MemTable
)

# Register Catch2 tests with CTest
include(Catch)
catch_discover_tests(MemTableTest)
