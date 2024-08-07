cmake_minimum_required(VERSION 3.25)
project(zkv)

add_executable(Main "main.cpp")

set_target_properties(Main PROPERTIES CXX_STANDARD 23)
target_include_directories(Main INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Main spdlog::spdlog fmt::fmt nlohmann_json::nlohmann_json nlohmann_json_schema_validator cxxopts::cxxopts DB LSMTree MemTable Config HashIndex)
