# fast_tpx3_parser

`fast_tpx3_parser` is an experimental C++ project for reading TPX3 data files
and dispatching hit and trigger words through bounded, concurrent queues. Its
current focus is parser, batching, and sorting components rather than a
standalone command-line application.

## Requirements

- CMake 3.31 or newer
- A C++26-capable compiler
- Conan 2 and Ninja
- The dependencies declared in [`conanfile.py`](conanfile.py): Catch2,
  Google Benchmark, flux, and libassert (CMake also requires GTest)

## Build and test

From the repository root:

```sh
conan install . \
  --output-folder=build/debug \
  --build=missing \
  -s build_type=Debug

cmake -S . -B build/debug \
  -DCMAKE_TOOLCHAIN_FILE=build/debug/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -G Ninja

cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

The CMake project builds the `mylib` library, the `unit_tests` test executable,
and the `benchmarks` executable. There is currently no configured application
or CLI target.

## Benchmarks

Benchmarks use generated in-memory workloads; no TPX3 data file is required.
They cover header dispatch, trigger parsing and its worker thread, sorting,
trigger and hit merging, delay processing, the sink thread, and clock
correction. Run them with:

```sh
./build/debug/benchmarks
```

## Documentation

See [`docs/architecture.md`](docs/architecture.md) for component descriptions,
data flow, and notes on which parts of the pipeline are currently connected.
