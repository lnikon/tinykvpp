if(GTest_FOUND)
    enable_testing()

    add_executable(assert_test assert_test.cpp)
    target_link_libraries(assert_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(assert_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME AssertTest COMMAND assert_test)

    add_executable(arena_test arena_test.cpp)
    target_link_libraries(arena_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(arena_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME ArenaTest COMMAND arena_test)

    add_executable(skiplist_test skiplist_test.cpp test_common.cpp)
    target_link_libraries(skiplist_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(skiplist_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME SkiplistTest COMMAND skiplist_test)

    add_executable(memtable_test memtable_test.cpp test_common.cpp)
    target_link_libraries(memtable_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(memtable_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME MemtableTest COMMAND memtable_test)

    add_executable(wal_test wal_test.cpp)
    target_link_libraries(wal_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(wal_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME WalTest COMMAND wal_test)

    add_executable(fs_test fs_test.cpp)
    target_link_libraries(fs_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(fs_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME FsTest COMMAND fs_test)

    add_executable(engine_test engine_test.cpp)
    target_link_libraries(engine_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(engine_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME EngineTest COMMAND engine_test)

    add_executable(time_test time_test.cpp)
    target_link_libraries(time_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(time_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME TimeTest COMMAND time_test)

    message(STATUS "GTest found - testing enabled")
else()
    message(STATUS "GTest not found - testing disabled")
endif()

if(benchmark_FOUND)
    add_executable(skiplist_benchmark skiplist_benchmark.cpp test_common.cpp)
    target_link_libraries(
        skiplist_benchmark
        PRIVATE frankie_lib benchmark::benchmark_main
    )
    target_include_directories(
        skiplist_benchmark
        PRIVATE ${CMAKE_SOURCE_DIR}/src
    )

    add_executable(memtable_benchmark memtable_benchmark.cpp)
    target_link_libraries(
        memtable_benchmark
        PRIVATE frankie_lib benchmark::benchmark_main
    )
    target_include_directories(
        memtable_benchmark
        PRIVATE ${CMAKE_SOURCE_DIR}/src
    )

    message(STATUS "GoogleBenchmark found - benchmarks are enabled")
else()
    message(STATUS "GoogleBenchmark not found - benchmarks are disabled")
endif()
