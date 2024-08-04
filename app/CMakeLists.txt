cmake_minimum_required(VERSION 3.25)
project(zkv)

add_executable(AppMain "main.cpp")

set_target_properties(AppMain PROPERTIES CXX_STANDARD 23)
target_include_directories(AppMain INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(AppMain spdlog::spdlog fmt::fmt nlohmann_json::nlohmann_json nlohmann_json_schema_validator DB LSMTree MemTable Config HashIndex)
