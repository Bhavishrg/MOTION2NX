# Match Quest -- Fast and Secure Pattern Matching

This directory contains the implementation of the secure pattern matching protocols of Match Quest. Our code is buildt on top of the MOTION2NX framework available at https://github.com/encryptogroup/MOTION2NX. We note that the code is still in its initial stages and under development.

## External Dependencies

- [GCC 11.1.0](https://gcc.gnu.org/) or [Clang/LLVM 12.0.1](https://clang.llvm.org/)
- [CMake 3.21.4](https://cmake.org/)
- [Boost 1.76.0](https://www.boost.org/)
- [fmt 8.0.1](https://github.com/fmtlib/fmt)
- [flatbuffers 2.0.0](https://github.com/google/flatbuffers)



Build project using 
```
$ CC=gcc CXX=g++ cmake \
    -B build_debwithrelinfo_gcc \
    -DCMAKE_BUILD_TYPE=DebWithRelInfo \
    -DMOTION_BUILD_EXE=On \
    -DMOTION_BUILD_TESTS=On \
    -DMOTION_USE_AVX=AVX2
$ cmake --build build_debwithrelinfo_gcc
```
##Usage

To test equality, run :
```
cd build_debwithrelinfo_gcc

./bin/equality --my-id 0 --party 0,0.0.0.0,7002 --party 1,0.0.0.0,7003 --repetitions 10 --num-simd 100 --ring-size 8

./bin/equality --my-id 1 --party 0,0.0.0.0,7002 --party 1,0.0.0.0,7003  --repetitions 10 --num-simd 100 --ring-size 8

```

`ring-size` denotes the input length (8, 32, 64, 256).

Use :
- `./bin/equality`: Benchmark the performance of the our equality protocol.
- `/bin/circuit_equality`: Benchmark the performance of the circuit based equality protocol.
- `/bin/dpf_equality`: Benchmark the performance of DPF based equality protocol.



To test pattern matching protocols, run :
```
./bin/exact_pm --my-id 0 --party 0,0.0.0.0,7002 --party 1,0.0.0.0,7003 --pattern-size 80 --text-size 800

./bin/exact_pm --my-id 1 --my-id 1 --party 0,0.0.0.0,7002 --party 1,0.0.0.0,7003  --pattern-size 80 --text-size 800

``` 

`pattern-size` and `text-size` are configurable.

Use :

- `exact_pm`: Benchmark the performance of our exact pattern matching protocol.
- `wildcard_pm`: Benchmark the performance of our wildcard pattern matching protocol.
- `approx_pm`: Benchmark the performance of our approximate pattern matching protocol .
- `naive_exact_pm`: Benchmark the performance of naive approximate pattern matching protocol.
- `naive_wildcard_pm`: Benchmark the performance of naive wildcard pattern matching.
- `naive_approx_pm2`: Benchmark the performance of naive approximate pattern matching protocol.


## Build Instructions


This software was developed and tested in the following environment (it might
also work with older versions):

- [Arch Linux](https://archlinux.org/)
- [GCC 11.1.0](https://gcc.gnu.org/) or [Clang/LLVM 12.0.1](https://clang.llvm.org/)
- [CMake 3.21.4](https://cmake.org/)
- [Boost 1.76.0](https://www.boost.org/)
- [OpenSSL 1.1.1.l](https://openssl.org/)
- [Eigen 3.4.0](https://eigen.tuxfamily.org/)
- [fmt 8.0.1](https://github.com/fmtlib/fmt)
- [flatbuffers 2.0.0](https://github.com/google/flatbuffers)
- [GoogleTest 1.11.0 (optional, for tests, build automatically)](https://github.com/google/googletest)
- [Google Benchmark 1.6.0 (optional, for some benchmarks, build automatically)](https://github.com/google/benchmark)
- [HyCC (optional, for the HyCCAdapter)](https://gitlab.com/securityengineering/HyCC)
- [ONNX 1.10.2 (optional, for the ONNXAdapter)](https://github.com/onnx/onnx)

The build system downloads and builds GoogleTest and Benchmark if required.
It also tries to download and build Boost, fmt, and flatbuffers if it cannot
find these libraries in the system.

The framework can for example be compiled as follows:
```
$ CC=gcc CXX=g++ cmake \
    -B build_debwithrelinfo_gcc \
    -DCMAKE_BUILD_TYPE=DebWithRelInfo \
    -DMOTION_BUILD_EXE=On \
    -DMOTION_BUILD_TESTS=On \
    -DMOTION_USE_AVX=AVX2
$ cmake --build build_debwithrelinfo_gcc
```
Explanation of the flags:

- `CC=gcc CXX=g++`: select GCC as compiler
- `-B build_debwithrelinfo_gcc`: create a build directory
- `-DCMAKE_BUILD_TYPE=DebWithRelInfo`: compile with optimization and also add
  debug symbols -- makes tests run faster and debugging easier
- `-DMOTION_BUILD_EXE=On`: build example executables and benchmarks
- `-DMOTION_BUILD_TESTS=On`: build tests
- `-DMOTION_USE_AVX=AVX2`: compile with AVX2 instructions (choose one of `AVX`/`AVX2`/`AVX512`)

### HyCC Support for Hybrid Circuits

To enable support for HyCC circuits, the HyCC library must be compiled and the
following flags need additionally be passed to CMake:

- `-DMOTION_BUILD_HYCC_ADAPTER=On`
- `-DMOTION_HYCC_PATH=/path/to/HyCC` where `/path/to/HyCC` points to the HyCC
  directory, i.e., the top-level directory of the cloned repository

This builds the library target `motion_hycc` and the `hycc2motion` executable.



### ONNX Support for Neural Networks

For ONNX support, the ONNX library must be installed and the following flag
needs additionally be passed to CMake:

- `-DMOTION_BUILD_ONNX_ADAPTER=On`

This builds the library target `motion_onnx` and the `onnx2motion` executable.
