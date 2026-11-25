cmake_minimum_required(VERSION 3.31)

project(MyProject
        VERSION 1.0.0
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG "-Og")
# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

find_package(libassert REQUIRED)

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

add_library(fastTPX3 STATIC
        src/lib/Utility/Memory_Map.cpp
        src/lib/Parsing/tpx3_helper.cpp
        src/lib/Utility/AdaptivBackoff.cpp
)

target_include_directories(fastTPX3
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(fastTPX3
        PUBLIC
        ${PROJECT_SOURCE_DIR}/src/lib
)

set_property(TARGET fastTPX3 PROPERTY COMPILE_WARNING_AS_ERROR ON)

target_compile_features(fastTPX3 PUBLIC cxx_std_26)
target_compile_options(fastTPX3 PUBLIC -mcx16
        -march=native
        -Wno-interference-size
        -Wno-deprecated-declarations
        -Wall
        -Wpedantic
        -Werror
)

# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

target_link_libraries(fastTPX3
        PUBLIC
        atomic
        libassert::assert
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(ENABLE_TESTS "Enable tests" OFF)
if(ENABLE_TESTS)
  message(STATUS "Tests enabled")
  enable_testing()

  find_package(Catch2 3 REQUIRED)

  add_executable(unit_tests
        src/lib/Batch/Batch_Pool_test.cpp
        src/lib/QueueUtility/Extractor_test.cpp
        src/lib/QueueUtility/Injector_test.cpp
        src/lib/QueueUtility/Sequencer_test.cpp
        src/lib/Batch/Batch_test.cpp
        src/lib/Operations/Sink_test.cpp
          src/lib/Parsing/ParseHeader_test.cpp
        src/lib/Parsing/ParseHits_test.cpp
        src/lib/Parsing/ParseTrigger_test.cpp
        src/lib/Parsing/tpx3_helper_test.cpp
        src/lib/QueueUtility/Iterator_test.cpp
        src/lib/Utility/AdaptivBackoff_test.cpp
        src/lib/Utility/Memory_Map_test.cpp
        src/lib/Utility/bits_test.cpp
        src/lib/Utility/helper_test.cpp
        src/lib/Utility/optional_test.cpp
        src/lib/Utility/clock_correction_test.cpp
        src/lib/Operations/Sorter_test.cpp
        src/lib/Operations/Worker_test.cpp
        src/lib/Operations/Pipeline_test.cpp

          src/Camera_test.cpp
          src/Rotation_test.cpp
          src/Rotation.cpp
  )

  target_link_libraries(unit_tests
        PRIVATE
        fastTPX3
        Catch2::Catch2WithMain
)

  include(CTest)
  include(Catch)

  catch_discover_tests(unit_tests)

endif()

# ---------------------------------------------------------------------------
# Benchmarks
# ---------------------------------------------------------------------------
option(ENABLE_BENCHMARKS "Enable benchmarks" OFF)
if(ENABLE_BENCHMARKS)
  find_package(benchmark REQUIRED)
  message(STATUS "Benchmarks enabled")
  add_executable(benchmarks
        benchmarks/benchmark.cpp
        benchmarks/bm_clock_correction.cpp
        benchmarks/bm_parse_trigger.cpp
        benchmarks/bm_sorter.cpp
        benchmarks/bm_clock_correction.cpp
        benchmarks/bm_sink.cpp
)

  target_link_libraries(benchmarks
        PRIVATE
        fastTPX3
        benchmark::benchmark
)
endif()

add_executable(main src/main.cpp src/Rotation.cpp)

target_link_libraries(main
        PRIVATE
        fastTPX3
)
