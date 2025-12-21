cmake_minimum_required(VERSION 3.25)
project(frankie)

add_library(
    Serialization
    buffer_writer.cpp
    buffer_reader.cpp
    crc32.cpp
    common.cpp
)
set_target_properties(Serialization PROPERTIES CXX_STANDARD 23)
target_include_directories(
    Serialization
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(Serialization libassert::assert)

# BufferReaderWriterTest
add_executable(BufferReaderWriterTest buffer_reader_writer_test.cpp)
set_target_properties(BufferReaderWriterTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(
    BufferReaderWriterTest
    gtest::gtest
    spdlog::spdlog
    fmt::fmt
    Serialization
)

include(GoogleTest)
gtest_discover_tests(BufferReaderWriterTest)

# EndianIntegerTest
add_executable(EndianIntegerTest endian_integer_test.cpp)
set_target_properties(EndianIntegerTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(
    EndianIntegerTest
    gtest::gtest
    spdlog::spdlog
    fmt::fmt
    Serialization
)

include(GoogleTest)
gtest_discover_tests(EndianIntegerTest)

# VarintTest
add_executable(VarintTest varint_test.cpp)
set_target_properties(VarintTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(
    VarintTest
    gtest::gtest
    spdlog::spdlog
    fmt::fmt
    Serialization
)

include(GoogleTest)
gtest_discover_tests(VarintTest)

# CRC32
add_executable(CRC32Test crc32_test.cpp)
set_target_properties(VarintTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(
    CRC32Test
    gtest::gtest
    spdlog::spdlog
    fmt::fmt
    Serialization
)

include(GoogleTest)
gtest_discover_tests(CRC32Test)
