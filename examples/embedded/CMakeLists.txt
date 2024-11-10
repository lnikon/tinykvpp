cmake_minimum_required(VERSION 3.25)
project(zkv)

add_executable(EmbeddedMain "main.cpp")
set_target_properties(EmbeddedMain PROPERTIES CXX_STANDARD 23)
target_include_directories(EmbeddedMain INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(EmbeddedMain DB)
