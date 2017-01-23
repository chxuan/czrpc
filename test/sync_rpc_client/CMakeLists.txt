cmake_minimum_required(VERSION 2.8)
project(sync_rpc_client)

set(OUTPUTNAME sync_rpc_client)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-deprecated -Wno-comment -Wno-unused-local-typedefs -Wno-maybe-uninitialized -Wno-unused-variable -g -O3 -std=c++14")

aux_source_directory(. DIR_SRCS)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/../..)
include_directories(${PROJECT_SOURCE_DIR}/../../czrpc)
include_directories(${PROJECT_SOURCE_DIR}/../../spdlog/include)
include_directories($ENV{BOOST_INCLUDE_PATH})

link_directories($ENV{BOOST_LIB_PATH})

add_executable(${OUTPUTNAME} ${DIR_SRCS})

target_link_libraries(${OUTPUTNAME} boost_serialization)
target_link_libraries(${OUTPUTNAME} boost_system)
target_link_libraries(${OUTPUTNAME} pthread)
