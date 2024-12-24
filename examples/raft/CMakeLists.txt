cmake_minimum_required(VERSION 3.25)
project(zkv)

add_executable(RaftMain "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
set_target_properties(RaftMain PROPERTIES CXX_STANDARD 23)
target_link_libraries(RaftMain PRIVATE DB RaftProtoObjects)
