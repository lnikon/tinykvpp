cmake_minimum_required(VERSION 3.18)
project(zkv)

add_library(LSMTree "ILSMTreeSegment.cpp"
                    "LSMTreeMockSegment.cpp"
                    "LSMTreeRegularSegment.cpp"
                    "LSMTree.cpp"
                    "LSMTreeSegmentFactory.cpp"
                    "LSMTreeSegmentManager.cpp"
                    "LSMTreeSegmentStorage.cpp")
target_include_directories(LSMTree INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(LSMTreeTest "LSMTreeTest.cpp")
target_link_libraries(LSMTreeTest boost::boost Catch2::Catch2 gRPC::grpc spdlog::spdlog fmt::fmt ZLIB::ZLIB LSMTree MemTable)
