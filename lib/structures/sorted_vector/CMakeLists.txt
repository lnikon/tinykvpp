cmake_minimum_required(VERSION 3.25)
project(zkv)

add_library(SortedVector "sorted_vector.cpp")
target_compile_features(SortedVector PUBLIC cxx_std_23)
target_include_directories(SortedVector INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
