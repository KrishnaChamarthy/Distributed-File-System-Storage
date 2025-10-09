cmake_minimum_required(VERSION 3.16)
project(ChunkServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # Generate compile_commands.json for VS Code

# Use pkg-config to find gRPC and protobuf
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)
pkg_check_modules(PROTOBUF REQUIRED protobuf)

# Find protoc executable
find_program(PROTOC_EXECUTABLE protoc REQUIRED)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)

# Proto file
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Proto/ChunkService.proto")
set(PROTO_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Proto")

# Generated files
set(PROTO_SRCS "${PROTO_SRC_DIR}/ChunkService.pb.cc")
set(PROTO_HDRS "${PROTO_SRC_DIR}/ChunkService.pb.h")
set(GRPC_SRCS "${PROTO_SRC_DIR}/ChunkService.grpc.pb.cc")
set(GRPC_HDRS "${PROTO_SRC_DIR}/ChunkService.grpc.pb.h")

# Custom command to generate protobuf files
add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
    COMMAND ${PROTOC_EXECUTABLE}
    ARGS --grpc_out "${PROTO_SRC_DIR}"
         --cpp_out "${PROTO_SRC_DIR}"
         -I "${PROTO_SRC_DIR}"
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
         "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}"
    COMMENT "Generating protobuf and gRPC files"
)

# Create executables
add_executable(my_app
    ChunkServer/Server.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

add_executable(client
    Client/Client.cpp
    Client/Uploader.cpp
    Client/Downloader.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

# Link libraries
target_link_libraries(my_app
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
)

target_link_libraries(client
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
)

# Explicitly add library directories
target_link_directories(my_app PRIVATE
    ${GRPC_LIBRARY_DIRS}
    ${PROTOBUF_LIBRARY_DIRS}
    /opt/homebrew/lib
)

target_link_directories(client PRIVATE
    ${GRPC_LIBRARY_DIRS}
    ${PROTOBUF_LIBRARY_DIRS}
    /opt/homebrew/lib
)

# Include directories and compile flags
target_include_directories(my_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROTO_SRC_DIR}
    ${GRPC_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIRS}
)

target_include_directories(client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROTO_SRC_DIR}
    ${GRPC_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIRS}
)

target_compile_options(my_app PRIVATE
    ${GRPC_CFLAGS_OTHER}
    ${PROTOBUF_CFLAGS_OTHER}
)

target_compile_options(client PRIVATE
    ${GRPC_CFLAGS_OTHER}
    ${PROTOBUF_CFLAGS_OTHER}
)