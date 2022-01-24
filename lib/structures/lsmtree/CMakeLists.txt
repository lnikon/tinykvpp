cmake_minimum_required(VERSION 3.18)
project(zkv)

add_library(LSMTree "LSMTree.cpp")
target_include_directories(LSMTree INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(LSMTreeTest "LSMTree.cpp")
target_link_libraries(LSMTreeTest PRIVATE LSMTree ${CONAN_LIBS})