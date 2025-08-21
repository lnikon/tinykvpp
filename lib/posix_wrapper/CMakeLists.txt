cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(POSIXWrapper open_flag.cpp)

set_target_properties(POSIXWrapper PROPERTIES CXX_STANDARD 23)
target_include_directories(POSIXWrapper INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(POSIXWrapper PUBLIC uring spdlog::spdlog)

add_executable(POSIXWrapperTest posix_wrapper_test.cpp)
set_target_properties(POSIXWrapperTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(POSIXWrapperTest gtest::gtest)
