# fast_tpx3_parser

`fast_tpx3_parser` is an experimental C++ project for reading TPX3 data files
and dispatching hit and trigger words through bounded, concurrent queues. Its
current focus is parser, batching, and sorting components rather than a
standalone command-line application.

## Requirements

- CMake 3.31 or newer
- A C++26-capable compiler
- Conan 2 and Ninja
- libassert for the library; Catch2, Google Test, and Google Benchmark for the
  optional tests and tools

## Build

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
```

The project builds the `fast_tpx3_parser` library. Unit tests and the parser
executable/benchmarks are enabled by default when building this repository;
disable them when embedding the project with `-DBUILD_TESTING=OFF
-DFAST_TPX3_PARSER_BUILD_TOOLS=OFF`.

Run the unit tests with:

```sh
ctest --test-dir build/debug --output-on-failure
```

## Install and use from another CMake project

Install the library and CMake package files:

```sh
cmake --install build/debug --prefix install
```

Then consume the exported target from another project:

```cmake
find_package(fast_tpx3_parser CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE fast_tpx3_parser::fast_tpx3_parser)
```

## Benchmarks

The benchmark cases read `./tpx3.tpx3` from the current working directory.
Provide a TPX3 data file at that path before running the benchmark:

```sh
./build/debug/benchmarks
```

No input data file is included in the repository.

## Documentation

See [`docs/architecture.md`](docs/architecture.md) for component descriptions,
data flow, and notes on which parts of the pipeline are currently connected.
