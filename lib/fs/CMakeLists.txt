cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(FS append_only_file.cpp random_access_file.cpp common.h common.cpp)

set_target_properties(FS PROPERTIES CXX_STANDARD 23)
target_include_directories(FS INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(FS PUBLIC uring spdlog::spdlog)

# add_executable(DBTest "db_test.cpp") set_target_properties(DBTest PROPERTIES
# CXX_STANDARD 23) target_link_libraries( DBTest Catch2::Catch2WithMain
# spdlog::spdlog fmt::fmt LSMTree MemTable DB Config HashIndex)
