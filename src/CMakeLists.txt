cmake_minimum_required(VERSION 3.25)
project(zkv)

add_executable(Main "main.cpp")

set_target_properties(Main PROPERTIES CXX_STANDARD 23)
target_include_directories(Main INTERFACE ${proto_import_dirs} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Main DB fmt::fmt)

#target_include_directories(GrpcAppMain INTERFACE  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

