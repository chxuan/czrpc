cmake_minimum_required(VERSION 2.8)
project(pub_client)

set(OUTPUTNAME pub_client)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-deprecated -Wno-comment -Wno-unused-local-typedefs -Wno-maybe-uninitialized -Wno-unused-variable -g -O3 -std=c++14")

aux_source_directory(. DIR_SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/../proto/code DIR_SRCS)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/../..)
include_directories(${PROJECT_SOURCE_DIR}/../../czrpc)
include_directories(${PROJECT_SOURCE_DIR}/../proto/code)
include_directories($ENV{BOOST_INCLUDE_PATH})
include_directories($ENV{PROTOBUF_INCLUDE_PATH})

link_directories($ENV{BOOST_LIB_PATH})
link_directories($ENV{PROTOBUF_LIB_PATH})

add_executable(${OUTPUTNAME} ${DIR_SRCS})

target_link_libraries(${OUTPUTNAME} boost_system.a)
target_link_libraries(${OUTPUTNAME} libprotobuf.a)
target_link_libraries(${OUTPUTNAME} pthread)
