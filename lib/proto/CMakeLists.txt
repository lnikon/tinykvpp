cmake_minimum_required(VERSION 3.25)
project(zkv)

set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(PROTO_IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}")

add_library(TKVProtoObjects OBJECT "${CMAKE_CURRENT_LIST_DIR}/TinyKVPP.proto")
target_include_directories(TKVProtoObjects PUBLIC "$<BUILD_INTERFACE:${PROTO_BINARY_DIR}>" ${protobuf_INCLUDE_DIR})
target_link_libraries(TKVProtoObjects PUBLIC protobuf::libprotobuf gRPC::grpc++)

protobuf_generate(
    TARGET TKVProtoObjects
    IMPORT_DIRS ${PROTO_IMPORT_DIRS}
    PROTOC_OUT_DIR "${PROTO_BINARY_DIR}"
)

protobuf_generate(
    TARGET TKVProtoObjects
    LANGUAGE grpc
    PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
    PLUGIN_OPTIONS generate_mock_code=true
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc)


