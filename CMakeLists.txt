cmake_minimum_required(VERSION 3.25)
project(zkv)

set(CMAKE_EXPORT_COMPILE_COMMANDS On)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

# Enable ASAN for all targets
# set(CMAKE_BUILD_TYPE "Debug")
# -std=c++23 -Wall -Wextra -pedantic-errors -stdlib=libstdc++ -lstdc++_libbacktrace -fsized-deallocation -no-pie
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=thread  -Wall -Wextra -pedantic-errors")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=thread  -Wall -Wextra -pedantic-errors")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=thread  -Wall -Wextra -pedantic-errors")

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

include_directories(lib)

add_subdirectory(lib)
add_subdirectory(app)
add_subdirectory(src)
add_subdirectory(bench)
add_subdirectory(examples)
# add_subdirectory(grpcapp)

# Custom targets to build docker images and run tests
add_custom_target(BuildGCCReleaseDockerImage
    COMMAND sh scripts/build_gcc_release_docker_image.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(BuildClangReleaseDockerImage
    COMMAND sh scripts/build_clang_release_docker_image.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
