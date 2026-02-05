cmake_minimum_required(VERSION 3.25)
project(frankie VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    )
endif()

# target_link_libraries(YOUR_TARGET gtest::gtest)
# target_link_libraries(YOUR_TARGET benchmark::benchmark_main)

# Runtime dependencies
find_package(liburing REQUIRED)
find_package(gRPC REQUIRED)
find_package(protobuf REQUIRED)
find_package(zstd REQUIRED)
find_package(lz4 REQUIRED)
find_package(OpenSSL REQUIRED)

# Development dependencies (optional)
find_package(benchmark REQUIRED)
find_package(GTest REQUIRED)

add_library(frankie_lib INTERFACE)
target_sources(frankie_lib INTERFACE src/storage/skiplist.cpp)
target_include_directories(
    frankie_lib
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(frankie_lib INTERFACE liburing::liburing)
target_link_libraries(frankie_lib INTERFACE grpc::grpc)
target_link_libraries(frankie_lib INTERFACE protobuf::protobuf)
target_link_libraries(frankie_lib INTERFACE zstd::libzstd_static)
target_link_libraries(frankie_lib INTERFACE LZ4::lz4_static)
target_link_libraries(frankie_lib INTERFACE openssl::openssl)

add_executable(frankie ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
target_link_libraries(frankie frankie_lib)

if(GTest_FOUND)
    enable_testing()

    # add_executable(main_test src/main_test.cpp)
    # target_link_libraries(main_test PRIVATE frankie_lib GTest::gtest_main)
    # add_test(NAME MainTest COMMAND main_test)

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)

    message(STATUS "GTest found - testing enabled")
else()
    message(STATUS "GTest not found - testing disabled")
endif()

if(benchmark_FOUND)
    # add_executable(main_bench src/main_bench.cpp)
    # target_link_libraries(main_bench PRIVATE frankie_lib benchmark::benchmark)

    message(STATUS "GoogleBenchmark found - benchmarks are enabled")
else()
    message(STATUS "GoogleBenchmark not found - benchmarks are disabled")
endif()
