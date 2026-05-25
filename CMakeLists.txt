# cmake_minimum_required (VERSION 3.15)

project(RLWETEST)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-O2 -Wall -Wfatal-errors -flto -mavx512f -mavx512dq -mavx512vl")

find_package(SEAL REQUIRED HINTS "${CMAKE_SOURCE_DIR}/../install/SEAL")

add_executable(example exec/example.cpp)
target_link_libraries(example SEAL::seal)