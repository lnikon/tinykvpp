cmake_minimum_required(VERSION 3.25)
project(zkv)

set(CMAKE_EXPORT_COMPILE_COMMANDS On)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

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

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

include_directories(lib)

add_subdirectory(lib)
add_subdirectory(app)
add_subdirectory(src)
add_subdirectory(bench)
# add_subdirectory(grpcapp)

# Custom targets to build docker images and run tests
add_custom_target(build_gcc_release
    COMMAND sh -c "docker build --build-arg TARGET=gcc . --file Dockerfile --tag tinykvpp-clang"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
