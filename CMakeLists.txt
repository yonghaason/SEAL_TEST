cmake_minimum_required(VERSION 3.15)

project(SEAL_LECTURE LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-O2 -Wall -Wfatal-errors -flto -mavx512f -mavx512dq -mavx512vl")

# 기본으로 ../seal-install 에 설치된 SEAL 을 사용한다.
# 다른 위치를 쓰려면 -DCMAKE_PREFIX_PATH=/path/to/seal-install 로 지정한다.
find_package(SEAL REQUIRED HINTS "${CMAKE_SOURCE_DIR}/../seal-install")

# 실험마다 실행 파일 하나: exec/<이름>.cpp -> build/<이름>
# -DSOL=ON 을 주면 solutions/<이름>.cpp 가 있는 실험은 그 파일로 빌드한다.
#   예: cmake -S . -B build-sol -DSOL=ON && cmake --build build-sol
option(SOL "Build from solutions/ instead of exec/" OFF)

set(EXECUTABLES
    sanitycheck
    exp0_params
    exp1_noise
    exp2_modswitch
    exp3_lazy
    exp4_rotation
    exp5_ckks
    exp6_logreg
    exp7_psi_basic
    exp8_psi_powers
    exp9_psi_bucket
    psi_hashing
)
foreach(name ${EXECUTABLES})
    set(src exec/${name}.cpp)
    if(SOL AND EXISTS "${CMAKE_SOURCE_DIR}/solutions/${name}.cpp")
        set(src solutions/${name}.cpp)
    endif()
    add_executable(${name} ${src})
    target_include_directories(${name} PRIVATE exec)
    target_link_libraries(${name} SEAL::seal)
endforeach()
if(SOL)
    message(STATUS "SOL=ON: building from solutions/")
endif()
