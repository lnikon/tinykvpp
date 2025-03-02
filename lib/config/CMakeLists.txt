cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(Config "config.cpp")

set_target_properties(Config PROPERTIES CXX_STANDARD 23)
target_include_directories(Config INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# add_executable(ConfigTest "db_test.cpp") set_target_properties(ConfigTest
# PROPERTIES CXX_STANDARD 23) target_link_libraries( ConfigTest boost::boost
# Catch2::Catch2 spdlog::spdlog fmt::fmt LSMTree MemTable
# Config)
