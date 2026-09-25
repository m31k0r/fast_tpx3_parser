cmake_minimum_required(VERSION 3.31)

project(MyProject
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)
find_package(flux CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

add_library(mylib
src/Rotation.cpp src/Memory_Map.cpp src/tpx3_helper.cpp
)

target_include_directories(mylib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(mylib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(mylib PUBLIC cxx_std_26)
target_compile_options(mylib PUBLIC -mcx16
)

target_link_libraries(mylib
    PUBLIC
        flux::flux
        atomic
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

enable_testing()

include(GoogleTest)

add_executable(unit_tests
tests/Batch_Pool_test.cpp
)

target_link_libraries(unit_tests
    PRIVATE
        mylib
        GTest::gtest_main
)

gtest_discover_tests(unit_tests)

# ---------------------------------------------------------------------------
# Benchmarks
# ---------------------------------------------------------------------------

add_executable(benchmarks
    benchmarks/benchmark.cpp
)

target_link_libraries(benchmarks
    PRIVATE
        mylib
        benchmark::benchmark
)

