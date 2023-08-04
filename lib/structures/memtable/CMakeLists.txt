cmake_minimum_required(VERSION 3.18)
project(zkv)

add_library(MemTable "memtable.cpp")
target_compile_features(MemTable PUBLIC cxx_std_23)
target_include_directories(MemTable INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(MemTableTest "memtable_test.cpp")
target_compile_features(MemTableTest PUBLIC cxx_std_23)
target_link_libraries(MemTableTest boost::boost Catch2::Catch2 gRPC::grpc spdlog::spdlog fmt::fmt ZLIB::ZLIB LSMTree MemTable)
