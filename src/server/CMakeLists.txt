cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(Server server_kind.cpp grpc_server.cpp)
target_include_directories(Server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(Server DB Config fmt::fmt)
