cmake_minimum_required(VERSION 2.8)
project(samples)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-function -Wno-deprecated -Wno-comment -Wno-unused-local-typedefs -Wno-maybe-uninitialized -Wno-unused-variable -g -O3 -std=c++14")
set(PROTO_LIST ${PROJECT_SOURCE_DIR}/proto/code/common.pb.cc)
set(LINK_LIST boost_system.a libprotobuf.a pthread)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/..)
include_directories(${PROJECT_SOURCE_DIR}/../czrpc)
include_directories(${PROJECT_SOURCE_DIR}/proto/code)
include_directories($ENV{BOOST_INCLUDE_PATH})
include_directories($ENV{PROTOBUF_INCLUDE_PATH})

link_directories($ENV{BOOST_LIB_PATH})
link_directories($ENV{PROTOBUF_LIB_PATH})

add_executable(sample1_client sample1_client.cpp ${PROTO_LIST})
target_link_libraries(sample1_client ${LINK_LIST})

add_executable(sample1_server sample1_server.cpp ${PROTO_LIST})
target_link_libraries(sample1_server ${LINK_LIST})

add_executable(sample2_server sample2_server.cpp ${PROTO_LIST})
target_link_libraries(sample2_server ${LINK_LIST})

add_executable(sample3_client sample3_client.cpp ${PROTO_LIST})
target_link_libraries(sample3_client ${LINK_LIST})

add_executable(sample3_server sample3_server.cpp ${PROTO_LIST})
target_link_libraries(sample3_server ${LINK_LIST})

add_executable(sample4_server sample4_server.cpp ${PROTO_LIST})
target_link_libraries(sample4_server ${LINK_LIST})

add_executable(sample5_server sample5_server.cpp ${PROTO_LIST})
target_link_libraries(sample5_server ${LINK_LIST})

add_executable(sample6_client sample6_client.cpp ${PROTO_LIST})
target_link_libraries(sample6_client ${LINK_LIST})

add_executable(sample6_server sample6_server.cpp ${PROTO_LIST})
target_link_libraries(sample6_server ${LINK_LIST})

add_executable(sample7_client sample7_client.cpp ${PROTO_LIST})
target_link_libraries(sample7_client ${LINK_LIST})

add_executable(sample8_client sample8_client.cpp ${PROTO_LIST})
target_link_libraries(sample8_client ${LINK_LIST})

add_executable(sample8_server sample8_server.cpp ${PROTO_LIST})
target_link_libraries(sample8_server ${LINK_LIST})
