cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(DB db.cpp db_config.cpp manifest/manifest.cpp)

set_target_properties(DB PROPERTIES CXX_STANDARD 23)
target_include_directories(DB INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  DB
  PUBLIC abseil::abseil
         spdlog::spdlog
         fmt::fmt
         nlohmann_json::nlohmann_json
         nlohmann_json_schema_validator
         cxxopts::cxxopts
         gRPC::grpc
         libassert::assert
         magic_enum::magic_enum
         TKVProtoObjects
         LSMTree
         MemTable
         Config
         HashIndex
         WAL)

add_executable(DBTest db_test.cpp)
set_target_properties(DBTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(
  DBTest
  gtest::gtest
  spdlog::spdlog
  fmt::fmt
  LSMTree
  MemTable
  DB
  Config
  HashIndex)

include(GoogleTest)
gtest_discover_tests(DBTest)
