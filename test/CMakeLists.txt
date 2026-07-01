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

    add_executable(sstable_writer_test sstable_writer_test.cpp)
    target_link_libraries(sstable_writer_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(sstable_writer_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME SstableWriterTest COMMAND sstable_writer_test)

    add_executable(time_test time_test.cpp)
    target_link_libraries(time_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(time_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME TimeTest COMMAND time_test)

    add_executable(codec_test codec_test.cpp)
    target_link_libraries(codec_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(codec_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME CodecTest COMMAND codec_test)

    add_executable(views_test views_test.cpp)
    target_link_libraries(views_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(views_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME ViewsTest COMMAND views_test)

    add_executable(crc32_test crc32_test.cpp)
    target_link_libraries(crc32_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(crc32_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME Crc32Test COMMAND crc32_test)

    add_executable(buffer_reader_writer_test buffer_reader_writer_test.cpp)
    target_link_libraries(buffer_reader_writer_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(buffer_reader_writer_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME BufferReaderWriterTest COMMAND buffer_reader_writer_test)

    add_executable(sstable_format_test sstable_format_test.cpp)
    target_link_libraries(sstable_format_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(sstable_format_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME SstableFormatTest COMMAND sstable_format_test)

    add_executable(segment_test segment_test.cpp)
    target_link_libraries(segment_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(segment_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME SegmentTest COMMAND segment_test)

    add_executable(sstable_reader_test sstable_reader_test.cpp)
    target_link_libraries(sstable_reader_test PRIVATE frankie_lib GTest::gtest_main)
    target_include_directories(sstable_reader_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME SstableReaderTest COMMAND sstable_reader_test)

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
