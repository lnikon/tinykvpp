cmake_minimum_required(VERSION 3.25)
project(zkv)

add_executable(basic_bench "basic_bench.cpp")
target_link_libraries(basic_bench benchmark::benchmark DB MemTable LSMTree Config HashIndex)

add_executable(celero_bench "celero_bench.cpp")
target_link_libraries(celero_bench celero spdlog::spdlog DB MemTable LSMTree Config HashIndex)

add_executable(gbench_bench "gbench_bench.cpp")
target_link_libraries(gbench_bench benchmark::benchmark spdlog::spdlog DB MemTable LSMTree Config HashIndex)

