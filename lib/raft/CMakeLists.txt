cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(Raft raft.cpp)
set_target_properties(Raft PROPERTIES CXX_STANDARD 23)
target_link_libraries(
  Raft
  RaftProtoObjects
  FS
  WAL
  libassert::assert
  spdlog::spdlog
  fmt::fmt
  magic_enum::magic_enum)

add_executable(RaftTest "raft_test.cpp")
set_target_properties(RaftTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(RaftTest gtest::gtest Raft RaftProtoObjects
                      TKVProtoObjects Concurrency)

include(GoogleTest)
gtest_discover_tests(RaftTest)
