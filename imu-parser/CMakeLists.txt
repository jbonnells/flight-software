cmake_minimum_required(VERSION 3.10)
project(imu_parser)

set(CMAKE_CXX_STANDARD 23)

add_executable(imu_parser.o ImuParser.cpp)

target_include_directories(imu_parser.o PRIVATE /usr/include)

target_link_libraries(imu_parser.o PRIVATE pthread)