cmake_minimum_required(VERSION 3.25)
project(zkv)

set(APP_NAME grpcapp)
set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(PROTO_IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}")

add_library(tinykvpp-proto-objects OBJECT "${CMAKE_CURRENT_LIST_DIR}/schemas/TinyKVPP.proto")
target_include_directories(tinykvpp-proto-objects PUBLIC "$<BUILD_INTERFACE:${PROTO_BINARY_DIR}>" ${protobuf_INCLUDE_DIR})
target_link_libraries(tinykvpp-proto-objects PUBLIC protobuf::libprotobuf gRPC::grpc++)

protobuf_generate(
    TARGET tinykvpp-proto-objects
    IMPORT_DIRS ${PROTO_IMPORT_DIRS}
    PROTOC_OUT_DIR "${PROTO_BINARY_DIR}"
)

protobuf_generate(
    TARGET tinykvpp-proto-objects
    LANGUAGE grpc
    PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
    PLUGIN_OPTIONS generate_mock_code=true
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc)

add_executable(GrpcAppMain "main.cpp")
set_target_properties(GrpcAppMain PROPERTIES CXX_STANDARD 23)
target_include_directories(GrpcAppMain INTERFACE ${PROTO_IMPORT_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(
    GrpcAppMain
    systemd
    gRPC::grpc
    spdlog::spdlog
    fmt::fmt
    ZLIB::ZLIB
    DB
    LSMTree
    MemTable
    Config
    HashIndex
    tinykvpp-proto-objects)
#add_definitions(-DHAVE_LIBSYSTEMD)

