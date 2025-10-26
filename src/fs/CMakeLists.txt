cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(FS append_only_file.cpp random_access_file.cpp common.cpp)

set_target_properties(FS PROPERTIES CXX_STANDARD 23)
target_include_directories(FS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(
    FS
    PUBLIC liburing::liburing spdlog::spdlog magic_enum::magic_enum POSIXWrapper
)

add_executable(FSTest random_access_file_test.cpp)
set_target_properties(FSTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(
    FSTest
    gtest::gtest
    FS
    liburing::liburing
    spdlog::spdlog
    POSIXWrapper
)
