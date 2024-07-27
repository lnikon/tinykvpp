cmake_minimum_required(VERSION 3.25)
project(zkv)

set(APP_NAME grpcapp)

# Black magic
# set(gRPC_USE_SYSTEMD "OFF")
# set(protobuf_MODULE_COMPATIBLE TRUE)
# set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
# set(_REFLECTION gRPC::grpc++_reflection)
# if(CMAKE_CROSSCOMPILING)
#   find_program(_PROTOBUF_PROTOC protoc)
# else()
#   set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
# endif()
#
# set(_GRPC_GRPCPP gRPC::grpc++)
# if(CMAKE_CROSSCOMPILING)
#   find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
# else()
#   set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
# endif()
#
# # Proto file
# get_filename_component(tinykvpp_proto "./schemas/TinyKVPP.proto" ABSOLUTE)
# get_filename_component(tinykvpp_proto_path "${tinykvpp_proto}" PATH)
#
# message(${tinykvpp_proto})
#
# # Generated sources
# set(tinykvpp_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/TinyKVPP.pb.cc")
# set(tinykvpp_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/TinyKVPP.pb.h")
# set(tinykvpp_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/TinyKVPP.grpc.pb.cc")
# set(tinykvpp_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/TinyKVPP.grpc.pb.h")
# add_custom_command(
#     OUTPUT "${tinykvpp_proto_srcs}" "${tinykvpp_proto_hdrs}" "${tinykvpp_grpc_srcs}" "${tinykvpp_grpc_hdrs}"
#     COMMAND ${_PROTOBUF_PROTOC}
#     ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
#     --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
#     -I "${tinykvpp_proto_path}"
#     --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
#     "${tinykvpp_proto}"
#     DEPENDS "${tinykvpp_proto}")
#
# # Include generated *.pb.h files
# include_directories("${CMAKE_CURRENT_BINARY_DIR}")
#
# message(${CMAKE_CURRENT_BINARY_DIR})
#
# # tinykvpp_grpc_proto
# add_library(tinykvpp_grpc_proto
#   ${tinykvpp_grpc_srcs}
#   ${tinykvpp_grpc_hdrs}
#   ${tinykvpp_proto_srcs}
#   ${tinykvpp_proto_hdrs})
# target_link_libraries(tinykvpp_grpc_proto
#   ${_REFLECTION}
#   ${_GRPC_GRPCPP}
#   ${_PROTOBUF_LIBPROTOBUF})
#

add_library(tinykvpp_proto_lib schemas/TinyKVPP.proto)
target_link_libraries(tinykvpp_proto_lib PUBLIC gRPC::grpc++)
target_include_directories(tinykvpp_proto_lib PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate(TARGET tinykvpp_proto_lib)
protobuf_generate(
    TARGET tinykvpp_proto_lib
    LANGUAGE grpc
    PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
    PLUGIN_OPTIONS generate_mock_code=true
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc)

add_executable(GrpcAppMain "main.cpp")
set_target_properties(GrpcAppMain PROPERTIES CXX_STANDARD 23)
target_include_directories(GrpcAppMain INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(
    GrpcAppMain systemd gRPC::grpc spdlog::spdlog fmt::fmt ZLIB::ZLIB DB LSMTree MemTable Config HashIndex tinykvpp_proto_lib)
add_definitions(-DHAVE_LIBSYSTEMD)

