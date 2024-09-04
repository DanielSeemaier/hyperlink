cmake_minimum_required(VERSION 3.20)
project(HyperlinkConverter)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(external/ips4o)

add_executable(txt2sbin txt2sbin.cc)
target_link_libraries(txt2sbin PUBLIC ips4o)

add_executable(revsbin revsbin.cc)
target_link_libraries(revsbin PUBLIC ips4o)

add_executable(edges2parhip edges2parhip.cc)

add_executable(edges2parhip64 edges2parhip64.cc)

