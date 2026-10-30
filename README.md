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

The benchmark cases read `./tpx3.tpx3` from the current working directory.
Provide a TPX3 data file at that path before running the benchmark:

```sh
./build/debug/benchmarks
```

No input data file is included in the repository.

`parser_benchmarks` measures each stage and thread of the `Parser`
pipeline (header dispatch, injector, trigger/hit parsing, sort and merge steps,
hit and trigger mergers, sink, and the worker threads) using synthetic data, so
it needs no input file:

```sh
./build/relwithdebinfo/parser_benchmarks --benchmark_filter='Merger|Sort'
```

Use a Release/RelWithDebInfo build for meaningful numbers.

## Documentation

See [`docs/architecture.md`](docs/architecture.md) for component descriptions,
data flow, and notes on which parts of the pipeline are currently connected.
