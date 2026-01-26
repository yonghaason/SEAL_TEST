### Build SEAL and main executables

```
cmake -S . -B build \
 -DSEAL_USE_INTEL_HEXL=ON \
 -DCMAKE_INSTALL_PREFIX=../install/SEAL
cmake --build build
cmake --install build
cmake -S . -B build
cmake --build build
cd build
make
'''