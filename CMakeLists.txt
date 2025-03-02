cmake_minimum_required(VERSION 3.25)
project(frankie)

set(CMAKE_EXPORT_COMPILE_COMMANDS On)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

set(TINYKVPP_COMMON_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic-errors")
set(TINYKVPP_COMMON_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic-errors"
)

message("Using compiler: ${CMAKE_CXX_COMPILER_ID}")

# CFLAGS="-fsanitize=thread -g -O1" CXXFLAGS="-fsanitize=thread -g -O1"
# LDFLAGS="-fsanitize=thread"

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS "${TINYKVPP_COMMON_C_FLAGS} -Wthread-safety")
    set(CMAKE_CXX_FLAGS "${TINYKVPP_COMMON_CXX_FLAGS} -Wthread-safety")
else()
    set(CMAKE_C_FLAGS "${TINYKVPP_COMMON_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${TINYKVPP_COMMON_CXX_FLAGS}")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

include(CTest)

find_package(Catch2 REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(nlohmann_json_schema_validator REQUIRED)
find_package(cxxopts REQUIRED)
find_package(benchmark REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(Celero CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(prometheus-cpp REQUIRED)

include_directories(lib)

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(bench)
add_subdirectory(examples)

# Custom targets to build docker images and run tests
add_custom_target(
    GCCReleaseDockerImage
    COMMAND sh scripts/build_gcc_release_docker_image.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(
    ClangReleaseDockerImage
    COMMAND sh scripts/build_clang_release_docker_image.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
