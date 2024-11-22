cmake_minimum_required(VERSION 3.25)
project(zkv)

set(PROTO_FILE "${CMAKE_CURRENT_LIST_DIR}/TinyKVPP.proto")
if(NOT EXISTS "${PROTO_FILE}")
  message(FATAL_ERROR "Proto file not found: ${PROTO_FILE}")
endif()

set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(PROTO_IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}")

add_library(TKVProtoObjects OBJECT "${PROTO_FILE}")
target_include_directories(TKVProtoObjects PUBLIC "$<BUILD_INTERFACE:${PROTO_BINARY_DIR}>" ${protobuf_INCLUDE_DIR})
target_link_libraries(TKVProtoObjects PUBLIC protobuf::libprotobuf gRPC::grpc++)

if(NOT TARGET gRPC::grpc_cpp_plugin)
  message(FATAL_ERROR "gRPC C++ plugin not found. Please gRPC is properly installed.")
endif()

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
