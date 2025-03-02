cmake_minimum_required(VERSION 3.25)
project(frankie)

add_executable(absl_try_mutex absl_try_mutex.cpp)
set_target_properties(absl_try_mutex PROPERTIES CXX_STANDARD 23)
target_include_directories(absl_try_mutex INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(absl_try_mutex abseil::abseil)