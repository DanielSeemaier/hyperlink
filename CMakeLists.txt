cmake_minimum_required(VERSION 3.20)
project(HyperlinkConverter)

set(CMAKE_CXX_STANDARD 20)

#find_package(Threads REQUIRED)

add_subdirectory(external/ips4o)

add_executable(txt2sbin txt2sbin.cc)
target_link_libraries(txt2sbin PUBLIC ips4o)
