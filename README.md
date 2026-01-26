### Build SEAL and main executables

```
git clone https://github.com/yonghaason/SEAL_TEST.git --recursive
cd SEAL_TEST/thirdparty/SEAL
cmake -S . -B build \
 -DSEAL_USE_INTEL_HEXL=ON \
 -DSEAL_BUILD_EXAMPLES=ON \
 -DSEAL_BUILD_BENCH=ON \
 -DCMAKE_INSTALL_PREFIX=../install/SEAL
cmake --build build
cmake --install build
cd ../..
cmake -S . -B build
cmake --build build
cd build
make
'''