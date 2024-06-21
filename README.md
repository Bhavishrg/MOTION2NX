# Match Quest - Fast and Secure Pattern Matching

This directory contains the implementation of the secure pattern matching protocols of Match Quest. Our code is buildt on top of the MOTION2NX framework available at https://github.com/encryptogroup/MOTION2NX. We note that the code is still in its initial stages and under development.

## External Dependencies

- [GCC 11.1.0](https://gcc.gnu.org/) or [Clang/LLVM 12.0.1](https://clang.llvm.org/)
- [CMake 3.21.4](https://cmake.org/)
- [Boost 1.76.0](https://www.boost.org/)
- [fmt 8.0.1](https://github.com/fmtlib/fmt)
- [flatbuffers 2.0.0](https://github.com/google/flatbuffers)



## Usage

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
