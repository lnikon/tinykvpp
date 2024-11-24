cmake_minimum_required(VERSION 3.25)
project(zkv)

add_library(Server "server_kind.cpp" "tcp_server.cpp" "grpc_server.cpp")
target_link_libraries(Server DB fmt::fmt)
