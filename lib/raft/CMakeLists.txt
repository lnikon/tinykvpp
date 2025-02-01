cmake_minimum_required(VERSION 3.25)
project(zkv)

add_library(RaftLib "raft.cpp")
set_target_properties(RaftLib PROPERTIES CXX_STANDARD 23)
target_link_libraries(RaftLib RaftProtoObjects TKVProtoObjects spdlog::spdlog)

add_executable(RaftTest "raft_test.cpp" "raft.cpp")
set_target_properties(RaftTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(RaftTest gtest::gtest DB
                      RaftProtoObjects TKVProtoObjects)
