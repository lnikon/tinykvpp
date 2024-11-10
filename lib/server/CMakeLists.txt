cmake_minimum_required(VERSION 3.25)
project(zkv)

add_library(Server "server.h")
target_link_libraries(Server DB)
