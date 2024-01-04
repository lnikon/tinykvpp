cmake_minimum_required(VERSION 3.27)
project(zkv)

add_library(
	HashIndex
  "hashindex.cpp")

set_target_properties(HashIndex PROPERTIES CXX_STANDARD 23)
target_include_directories(HashIndex INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# add_executable(HashIndexTest "hashindex_test.cpp")
# set_target_properties(HashIndexTest PROPERTIES CXX_STANDARD 23)
# target_link_libraries(
#   HashIndexTest
#   boost::boost
#   Catch2::Catch2
#   gRPC::grpc
#   spdlog::spdlog
#   fmt::fmt
#   ZLIB::ZLIB
# 	LSMTree
#   HashIndex
#   MemTable
# 	Config)
