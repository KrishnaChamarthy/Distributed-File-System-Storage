cmake_minimum_required(VERSION 3.15)
project(DistributedFileSystem CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)

# Find OpenSSL for encryption
find_package(OpenSSL REQUIRED)

# Find Threads
find_package(Threads REQUIRED)

# Add include directories
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/common)

# Generate protobuf and gRPC files
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(GENERATED_PROTOBUF_PATH "${CMAKE_CURRENT_BINARY_DIR}")

file(GLOB PROTO_FILES "${PROTO_PATH}/*.proto")

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    
    set(PROTO_SRCS "${GENERATED_PROTOBUF_PATH}/${PROTO_NAME}.pb.cc")
    set(PROTO_HDRS "${GENERATED_PROTOBUF_PATH}/${PROTO_NAME}.pb.h")
    set(GRPC_SRCS "${GENERATED_PROTOBUF_PATH}/${PROTO_NAME}.grpc.pb.cc")
    set(GRPC_HDRS "${GENERATED_PROTOBUF_PATH}/${PROTO_NAME}.grpc.pb.h")
    
    add_custom_command(
        OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
        COMMAND protoc
        ARGS --grpc_out=${GENERATED_PROTOBUF_PATH}
             --cpp_out=${GENERATED_PROTOBUF_PATH}
             --plugin=protoc-gen-grpc=`which grpc_cpp_plugin`
             -I${PROTO_PATH}
             ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating protobuf and gRPC files for ${PROTO_NAME}"
    )
    
    list(APPEND PROTO_GENERATED_FILES ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS})
endforeach()

# Common library with utilities
add_library(dfs_common
    src/common/utils.cpp
    src/common/crypto.cpp
    src/common/erasure_coding.cpp
    src/common/config.cpp
    ${PROTO_GENERATED_FILES}
)

target_link_libraries(dfs_common
    ${PROTOBUF_LIBRARIES}
    ${GRPC_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# Master server executable
add_executable(master_server
    src/master/master_server.cpp
    src/master/metadata_manager.cpp
    src/master/chunk_allocator.cpp
)

target_link_libraries(master_server dfs_common)

# Chunk server executable
add_executable(chunk_server
    src/chunkserver/chunk_server.cpp
    src/chunkserver/chunk_storage.cpp
)

target_link_libraries(chunk_server dfs_common)

# Client executable
add_executable(dfs_client
    src/client/client.cpp
)

target_link_libraries(dfs_client dfs_common)

# Web dashboard (simple HTTP server)
add_executable(web_dashboard
    src/master/web_server.cpp
)

target_link_libraries(web_dashboard dfs_common)

# Find testing frameworks (optional)
find_package(GTest QUIET)
find_package(GMock QUIET)

# Find jsoncpp for JSON handling
pkg_check_modules(JSONCPP jsoncpp)
if(JSONCPP_FOUND)
    target_link_libraries(dfs_common ${JSONCPP_LIBRARIES})
    target_include_directories(dfs_common PRIVATE ${JSONCPP_INCLUDE_DIRS})
endif()

# CLI executable (separate from client library)
add_executable(dfs_cli
    src/client/cli_main.cpp
    src/client/cli.cpp
)

target_link_libraries(dfs_cli dfs_common)

# Web server executable (separate from master)
add_executable(web_server
    src/master/web_server.cpp
)

target_link_libraries(web_server dfs_common)

# Tests (only if GTest is available)
if(GTest_FOUND AND GMock_FOUND)
    enable_testing()
    
    # Test framework library
    add_library(dfs_test_framework
        tests/test_framework.cpp
    )
    
    target_link_libraries(dfs_test_framework
        dfs_common
        GTest::gtest
        GTest::gmock
        Threads::Threads
    )
    
    target_include_directories(dfs_test_framework PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${GTEST_INCLUDE_DIRS}
        ${GMOCK_INCLUDE_DIRS}
    )
    
    # Individual test executables
    add_executable(crypto_test tests/crypto_test.cpp)
    target_link_libraries(crypto_test dfs_test_framework GTest::gtest_main)
    
    add_executable(erasure_coding_test tests/erasure_coding_test.cpp)
    target_link_libraries(erasure_coding_test dfs_test_framework GTest::gtest_main)
    
    add_executable(metadata_manager_test tests/metadata_manager_test.cpp)
    target_link_libraries(metadata_manager_test dfs_test_framework GTest::gtest_main)
    
    add_executable(integration_test tests/integration_test.cpp)
    target_link_libraries(integration_test dfs_test_framework GTest::gtest_main)
    
    # Register tests with CTest
    add_test(NAME CryptoTest COMMAND crypto_test)
    add_test(NAME ErasureCodingTest COMMAND erasure_coding_test)
    add_test(NAME MetadataManagerTest COMMAND metadata_manager_test)
    add_test(NAME IntegrationTest COMMAND integration_test)
    
    message(STATUS "Tests enabled - GTest found")
else()
    message(STATUS "Tests disabled - GTest not found. Install with: brew install googletest")
endif()

# Test data directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_data)

# Install targets
install(TARGETS master_server chunk_server dfs_client dfs_cli web_server
    DESTINATION bin
)

install(TARGETS dfs_common
    DESTINATION lib
)

# Install configuration files
install(DIRECTORY config/
    DESTINATION etc/dfs
    FILES_MATCHING PATTERN "*.json"
)

# Install scripts
install(DIRECTORY scripts/
    DESTINATION share/dfs/scripts
    USE_SOURCE_PERMISSIONS
)