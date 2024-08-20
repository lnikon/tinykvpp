cmake_minimum_required(VERSION 3.25)
project(zkv)

add_executable(open_write_streams "open_write_streams.cpp")
target_link_libraries(open_write_streams benchmark::benchmark)

add_executable(lots_of_write "lots_of_write.cpp")
target_link_libraries(lots_of_write benchmark::benchmark)
