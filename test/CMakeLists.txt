cmake_minimum_required(VERSION 3.25)
project(frankie VERSION 0.1.0 LANGUAGES CXX)

if(GTest_FOUND)
    enable_testing()

    add_executable(assert_test assert_test.cpp)
    target_link_libraries(assert_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(assert_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME AssertTest COMMAND assert_test)

    add_executable(skiplist_test skiplist_test.cpp test_common.cpp)
    target_link_libraries(skiplist_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(skiplist_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME SkiplistTest COMMAND skiplist_test)

    message(STATUS "GTest found - testing enabled")
else()
    message(STATUS "GTest not found - testing disabled")
endif()

if(benchmark_FOUND)
    add_executable(skiplist_benchmark skiplist_benchmark.cpp test_common.cpp)
    target_link_libraries(skiplist_benchmark PRIVATE frankie_lib benchmark::benchmark_main)
    target_include_directories(skiplist_benchmark PRIVATE ${CMAKE_SOURCE_DIR}/src)

    message(STATUS "GoogleBenchmark found - benchmarks are enabled")
else()
    message(STATUS "GoogleBenchmark not found - benchmarks are disabled")
endif()
