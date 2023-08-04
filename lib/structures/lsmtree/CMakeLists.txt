cmake_minimum_required(VERSION 3.27)
project(zkv)

add_library(LSMTree "ILSMTreeSegment.cpp"
                    "LSMTreeMockSegment.cpp"
                    "LSMTreeRegularSegment.cpp"
                    "LSMTree.cpp"
                    "LSMTreeSegmentFactory.cpp"
                    "LSMTreeSegmentManager.cpp"
                    "LSMTreeSegmentStorage.cpp")
set_target_properties(LSMTree PROPERTIES CXX_STANDARD 23)
target_include_directories(LSMTree INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(LSMTreeTest "LSMTreeTest.cpp")
set_target_properties(LSMTreeTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(LSMTreeTest boost::boost Catch2::Catch2 gRPC::grpc spdlog::spdlog fmt::fmt ZLIB::ZLIB LSMTree MemTable)
