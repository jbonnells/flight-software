cmake_minimum_required(VERSION 3.10)
project(server)

set(CMAKE_CXX_STANDARD 23)

add_executable(server.o server.cpp)

target_include_directories(server.o PRIVATE /usr/include)

target_link_libraries(server.o PRIVATE pthread)