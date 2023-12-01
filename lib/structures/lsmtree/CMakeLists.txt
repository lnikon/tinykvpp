cmake_minimum_required(VERSION 3.27)
project(zkv)

add_library(LSMTree "lsmtree.cpp"
                    "segments/interface_lsmtree_segment.cpp"
                    "segments/lsmtree_mock_segment.cpp"
                    "segments/lsmtree_regular_segment.cpp"
                    "segments/lsmtree_segment_factory.cpp"
                    "segments/lsmtree_segment_manager.cpp"
                    "segments/lsmtree_segment_storage.cpp")

set_target_properties(LSMTree PROPERTIES CXX_STANDARD 23)
target_include_directories(LSMTree INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(LSMTreeTest "lsmtree_test.cpp")
set_target_properties(LSMTreeTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(LSMTreeTest boost::boost Catch2::Catch2 gRPC::grpc spdlog::spdlog fmt::fmt ZLIB::ZLIB LSMTree MemTable)
