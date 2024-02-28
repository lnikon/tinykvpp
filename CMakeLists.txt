cmake_minimum_required(VERSION 3.18)
project(zkv)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD_REQUIRED True)
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
add_compile_options(-Wall -Wextra -pedantic -O0 -g)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

find_package(Boost REQUIRED)
find_package(Catch2 REQUIRED)
find_package(gRPC REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Protobuf REQUIRED)

include_directories(lib)
add_subdirectory(lib)
add_subdirectory(app)
add_subdirectory(grpcapp)

# TODO: Refactor
#set(sources "src/main.cpp")
#add_executable(main ${sources})
#target_link_libraries(main boost::boost Catch2::Catch2 gRPC::grpc spdlog::spdlog fmt::fmt ZLIB::ZLIB LSMTree MemTable)
