cmake_minimum_required(VERSION 3.25)
project(frankie)

add_executable(PrometheusMain "main.cpp")
set_target_properties(PrometheusMain PROPERTIES CXX_STANDARD 23)
target_link_libraries(PrometheusMain PRIVATE prometheus-cpp::prometheus-cpp)
