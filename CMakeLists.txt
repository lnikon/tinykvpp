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
        $<$<CONFIG:DEBUG>:-g3>
        $<$<CONFIG:DEBUG>:-O0>
        $<$<CONFIG:RELEASE>:-fno-omit-frame-pointer>
        $<$<CONFIG:RELEASE>:-O3>
    )
endif()

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

set(CORE_SRC
    src/core/arena.cpp
    src/core/scratch_arena.cpp
    src/core/simd.cpp
    src/core/crc32.cpp
    src/core/fs.cpp
    src/core/time.cpp
)
set(STORAGE_SRC src/storage/skiplist.cpp src/storage/memtable.cpp)
set(ENGINE_SRC src/engine/engine.cpp src/engine/wal.cpp)

add_library(frankie_lib STATIC ${CORE_SRC} ${STORAGE_SRC} ${ENGINE_SRC})
target_include_directories(frankie_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(frankie_lib PUBLIC liburing::liburing)
target_link_libraries(frankie_lib PUBLIC grpc::grpc)
target_link_libraries(frankie_lib PUBLIC protobuf::protobuf)
target_link_libraries(frankie_lib PUBLIC zstd::libzstd_static)
target_link_libraries(frankie_lib PUBLIC LZ4::lz4_static)
target_link_libraries(frankie_lib PUBLIC openssl::openssl)

add_executable(frankie ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
target_link_libraries(frankie frankie_lib)

if(GTest_FOUND)
    enable_testing()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
    message(STATUS "GTest found - testing enabled")
else()
    message(STATUS "GTest not found - testing disabled")
endif()

if(benchmark_FOUND)
    message(STATUS "GoogleBenchmark found - benchmarks are enabled")
else()
    message(STATUS "GoogleBenchmark not found - benchmarks are disabled")
endif()
