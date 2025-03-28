cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(
  WAL
  wal.h
  common.h
  common.cpp
  log/concepts.h
  log/in_memory_log_storage.h
  log/log.h
  log/persistent_log_storage.h)

set_target_properties(WAL PROPERTIES CXX_STANDARD 23)
target_include_directories(WAL INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  WAL
  PUBLIC abseil::abseil
         spdlog::spdlog
         fmt::fmt
         nlohmann_json::nlohmann_json
         nlohmann_json_schema_validator
         Config
         FS)

# add_executable(WALTest "wal_test.cpp") set_target_properties(WALTest
# PROPERTIES CXX_STANDARD 23) target_link_libraries( WALTest
# Catch2::Catch2WithMain spdlog::spdlog fmt::fmt LSMTree MemTable DB Config
# HashIndex )

# Register Catch2 tests with CTest include(Catch) catch_discover_tests(WALTest)
