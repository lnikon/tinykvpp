cmake_minimum_required(VERSION 3.25)
project(zkv)

add_executable(PrometheusMain "main.cpp")
set_target_properties(PrometheusMain PROPERTIES CXX_STANDARD 23)
target_link_libraries(PrometheusMain PRIVATE DB RaftProtoObjects TKVProtoObjects prometheus-cpp::prometheus-cpp)

# Register Catch2 tests with CTest
# include(Catch)
# catch_discover_tests(RaftTest)
