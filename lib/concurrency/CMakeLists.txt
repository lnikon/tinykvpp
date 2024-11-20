cmake_minimum_required(VERSION 3.25)
project(zkv)

add_library(Concurrency "thread_safe_queue.h" "thread_safe_queue.cpp")

set_target_properties(Concurrency PROPERTIES CXX_STANDARD 23)
target_include_directories(Concurrency INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Concurrency PUBLIC abseil::abseil spdlog::spdlog fmt::fmt)

# add_executable(DBTest "db_test.cpp")
# set_target_properties(DBTest PROPERTIES CXX_STANDARD 23)
# target_link_libraries(
#         DBTest
#         Catch2::Catch2WithMain
#         spdlog::spdlog
#         fmt::fmt
#         LSMTree
#         MemTable
#         DB
#         Config
#         HashIndex)
