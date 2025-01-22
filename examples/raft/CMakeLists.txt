cmake_minimum_required(VERSION 3.25)
project(zkv)

add_executable(RaftMain "main.cpp" "raft.cpp")
set_target_properties(RaftMain PROPERTIES CXX_STANDARD 23)
target_link_libraries(RaftMain PRIVATE DB RaftProtoObjects TKVProtoObjects)

add_executable(RaftTest "raft_test.cpp" "raft.cpp")
set_target_properties(RaftTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(RaftTest gtest::gtest DB
                      RaftProtoObjects TKVProtoObjects)

# Register Catch2 tests with CTest
# include(Catch)
# catch_discover_tests(RaftTest)
