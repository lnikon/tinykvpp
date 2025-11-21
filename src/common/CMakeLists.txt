cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(Common helpers.cpp)

set_target_properties(Common PROPERTIES CXX_STANDARD 23)
target_include_directories(Common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(
    Common
    PUBLIC
        libassert::assert
        fmt::fmt
        nlohmann_json::nlohmann_json
        nlohmann_json_schema_validator
        cxxopts::cxxopts
        magic_enum::magic_enum
)

# add_executable(ConfigTest "db_test.cpp") set_target_properties(ConfigTest
# PROPERTIES CXX_STANDARD 23) target_link_libraries( ConfigTest boost::boost
# Catch2::Catch2 spdlog::spdlog fmt::fmt LSMTree MemTable Config)
