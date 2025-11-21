cmake_minimum_required(VERSION 3.25)
project(frankie)

add_subdirectory(config)
add_subdirectory(db)
add_subdirectory(structures)
add_subdirectory(fs)
add_subdirectory(concurrency)
add_subdirectory(server)
add_subdirectory(raft)
add_subdirectory(wal)
add_subdirectory(posix_wrapper)
add_subdirectory(common)
add_subdirectory(serialization)

add_executable(Main main.cpp)
set_target_properties(Main PROPERTIES CXX_STANDARD 23)
target_include_directories(
    Main
    PUBLIC ${proto_import_dirs} ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    Main
    PUBLIC WAL DB Server Raft Config RaftProtoObjects
)

# target_include_directories(GrpcAppMain PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
# ${CMAKE_CURRENT_BINARY_DIR})
