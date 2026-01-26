cmake_minimum_required (VERSION 3.15)

project(RLWETEST)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-O2 -Wall -Wfatal-errors -flto -mavx512f -mavx512dq -mavx512vl")

find_package(SEAL 4.1 REQUIRED HINTS "${CMAKE_SOURCE_DIR}/thirdparty/install/SEAL")

add_executable(example0 exec/example0.cpp)
target_link_libraries(example0 SEAL::seal)

add_executable(example1 exec/example1.cpp)
target_link_libraries(example1 SEAL::seal)

add_executable(example2 exec/example2.cpp)
target_link_libraries(example2 SEAL::seal)