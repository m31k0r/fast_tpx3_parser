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
find_package(libassert REQUIRED)

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

add_library(mylib
        #src/Rotation.cpp
        src/Utility/Memory_Map.cpp
        src/Parsing/tpx3_helper.cpp
        src/Parser.cpp
        src/QueueUtility/Extractor.cpp
        src/QueueUtility/Injector.cpp
        src/Utility/bits.hpp
        src/Operations/MergeSPSC.hpp
        src/Parsing/ParseHits.hpp
        src/Parsing/ParseHeader.hpp
        src/Parsing/ParseTrigger.hpp
        src/Utility/AdaptivBackoff.cpp
        src/Utility/AdaptivBackoff.hpp
)

target_include_directories(mylib
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(mylib
        PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

set_property(TARGET mylib PROPERTY COMPILE_WARNING_AS_ERROR ON)

target_compile_features(mylib PUBLIC cxx_std_26)
target_compile_options(mylib PUBLIC -mcx16
        -march=native
        -Wno-interference-size
        -Wno-deprecated-declarations
        -Wall
        -Wpedantic
        -Werror
)

# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

target_link_libraries(mylib
        PUBLIC
        flux::flux
        atomic
        libassert::assert
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

enable_testing()

find_package(Catch2 3 REQUIRED)

add_executable(unit_tests
        src/Batch/Batch_Pool_test.cpp
        src/Parsing/tpx3_helper_test.cpp
        src/QueueUtility/Extractor_test.cpp
        src/QueueUtility/Injector_test.cpp
        src/QueueUtility/Sequencer_test.cpp
)

target_link_libraries(unit_tests
        PRIVATE
        mylib
        Catch2::Catch2WithMain
)

include(CTest)
include(Catch)

catch_discover_tests(unit_tests)

# ---------------------------------------------------------------------------
# Benchmarks
# ---------------------------------------------------------------------------

add_executable(benchmarks
        benchmarks/benchmark.cpp
        src/Operations/Worker.hpp
)

target_link_libraries(benchmarks
        PRIVATE
        mylib
        benchmark::benchmark
)


add_executable(main src/main.cpp)

target_link_libraries(main
        PRIVATE
        mylib
        benchmark::benchmark
)
