cmake_minimum_required(VERSION 3.27)
project(zkv)

add_library(DB "db.cpp" "manifest.cpp")

set_target_properties(DB PROPERTIES CXX_STANDARD 23)
target_include_directories(DB INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DB PRIVATE spdlog::spdlog)

add_executable(DBTest "db_test.cpp")
set_target_properties(DBTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(
        DBTest
        Catch2::Catch2WithMain
        spdlog::spdlog
        fmt::fmt
        LSMTree
        MemTable
        DB
        Config
        HashIndex)
