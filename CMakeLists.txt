cmake_minimum_required(VERSION 3.10)
project(ImuParser)

set(CMAKE_CXX_STANDARD 17)

add_executable(ImuParser ImuParser.cpp)

target_include_directories(ImuParser PRIVATE /usr/include)

target_link_libraries(ImuParser PRIVATE pthread)