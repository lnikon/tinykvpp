cmake_minimum_required(VERSION 3.25)
project(frankie)

add_executable(BasicBench basic_bench.cpp common.cpp)
target_link_libraries(BasicBench benchmark::benchmark DB)

add_executable(CeleroBench celero_bench.cpp common.cpp)
target_link_libraries(CeleroBench celero spdlog::spdlog DB)

add_executable(GoogleBench gbench_bench.cpp common.cpp)
target_link_libraries(GoogleBench benchmark::benchmark spdlog::spdlog DB)
