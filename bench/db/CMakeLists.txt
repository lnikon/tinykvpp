cmake_minimum_required(VERSION 3.25)
project(zkv)

add_executable(basic_bench "basic_bench.cpp")
target_link_libraries(basic_bench benchmark::benchmark DB MemTable LSMTree Config HashIndex)

