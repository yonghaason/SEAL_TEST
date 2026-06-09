cmake_minimum_required (VERSION 3.15)

project(RLWETEST)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-O2 -Wall -Wfatal-errors -flto -mavx512f -mavx512dq -mavx512vl")

find_package(SEAL REQUIRED HINTS "${CMAKE_SOURCE_DIR}/../../appcrypto26/install/SEAL/")

add_executable(example exec/example.cpp)
target_link_libraries(example SEAL::seal)

add_executable(ckks_logi_reg exec/ckks_logi_reg.cpp)
target_link_libraries(ckks_logi_reg SEAL::seal)

add_executable(assignment exec/assignment.cpp)
target_link_libraries(assignment SEAL::seal)
