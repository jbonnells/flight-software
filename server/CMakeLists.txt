cmake_minimum_required(VERSION 3.10)
project(server)

set(CMAKE_CXX_STANDARD 23)

add_executable(server server.cpp)

target_include_directories(server PRIVATE /usr/include)

target_link_libraries(server PRIVATE pthread)