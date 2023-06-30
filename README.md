# ExPat -- Express and Secure Pattern Matching

This software is the preliminary implementation of the paper "ExPat: Express and Secure Pattern Matching" which builds on the MOTION2NX framework available at https://github.com/encryptogroup/MOTION2NX.

# WARNING: This is not production-ready code.

This is software for a research prototype. Please
do *NOT* use this code in production.


# Build Instructions


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


# Testing the code

## Commands to check the working of equality.

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

Then run :
```
./build_debwithrelinfo_gcc/bin/equality --my-id 0 \
 --party 0,<PARTY_0_IP_ADDRESS> \
 --party 1,<PARTY_1_IP_ADDRESS> \
 --repetitions 1  --ring-size 16

 ./build_debwithrelinfo_gcc/bin/equality --my-id 1 \
 --party 0,<PARTY_0_IP_ADDRESS> \
 --party 1,<PARTY_1_IP_ADDRESS> \
 --repetitions 1  --ring-size 16

```


The `ring-size` option is configurable, and `my-id` option and IP addresses change accordingly.


## Commands to check the working of pattern matching protocols.

Build project and run

```
./build_debwithrelinfo_gcc/bin/exact_pmo --my-id 0 \
 --party 0,<PARTY_0_IP_ADDRESS> \
 --party 1,<PARTY_1_IP_ADDRESS> \
 --repetitions 1 --pattern-size 10 --text-size 100 --ring-size 16

 ./build_debwithrelinfo_gcc/bin/exact_pmo --my-id 1 \
 --party 0,<PARTY_0_IP_ADDRESS> \
 --party 1,<PARTY_1_IP_ADDRESS> \
 --repetitions 1 --pattern-size 10 --text-size 100 --ring-size 16

```
The `ring-size`, `text-size`, `pattern-size` and option is configurable, and `my-id` option and IP addresses change accordingly.

Similarly use, `./build_debwithrelinfo_gcc/bin/exact_pm` and `./build_debwithrelinfo_gcc/bin/exact_npm` to run exact patern matching without optimisation and the naive exact pattern matching protocol. Use `./build_debwithrelinfo_gcc/bin/wildcard_pmo`, `./build_debwithrelinfo_gcc/bin/wildcard_pm`, and `./build_debwithrelinfo_gcc/bin/wildcard_npm` for wildcard pattern matching.   

