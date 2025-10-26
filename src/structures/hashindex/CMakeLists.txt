cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(HashIndex hashindex.cpp)
set_target_properties(HashIndex PROPERTIES CXX_STANDARD 23)
target_include_directories(
    HashIndex
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# add_executable(HashIndexTest hashindex_test.cpp)
# set_target_properties(HashIndexTest PROPERTIES CXX_STANDARD 23)
# target_link_libraries(
#   HashIndexTest
#   Catch2::Catch2
#   spdlog::spdlog
#   fmt::fmt
# 	LSMTree
#   HashIndex
#   MemTable
# 	Config)
