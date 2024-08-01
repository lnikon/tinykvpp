cmake_minimum_required(VERSION 3.25)
project(zkv)

add_library(SortedVector "sorted_vector.cpp")
target_compile_features(SortedVector PUBLIC cxx_std_23)
target_include_directories(SortedVector INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(SortedVectorTest "sorted_vector_test.cpp")
set_target_properties(SortedVectorTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(
  SortedVectorTest
  Catch2::Catch2WithMain
  spdlog::spdlog
  fmt::fmt
	LSMTree
  SkipList
  MemTable
	Config
  SortedVector)
