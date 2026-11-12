cmake_minimum_required(VERSION 3.31)

project(fast_tpx3_parser
        VERSION 1.0.0
        LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(libassert CONFIG REQUIRED)

add_library(fast_tpx3_parser
        src/Parsing/Rotation.cpp
        src/Utility/Memory_Map.cpp
        src/Parsing/tpx3_helper.cpp
        src/Parser.cpp
        src/QueueUtility/Extractor.cpp
        src/QueueUtility/Injector.cpp
        src/Utility/AdaptivBackoff.cpp
        src/Parsing/Camera.cpp
)
add_library(fast_tpx3_parser::fast_tpx3_parser ALIAS fast_tpx3_parser)

target_sources(fast_tpx3_parser
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES
                src/Batch/Batch.hpp
                src/Batch/Batch_Pool.hpp
                src/Constants.hpp
                src/Operations/Delay.hpp
                src/Operations/Exporter.hpp
                src/Operations/MergeSPSC.hpp
                src/Operations/Merging_Buffer.hpp
                src/Operations/Sink.hpp
                src/Operations/Sorter.hpp
                src/Operations/Worker.hpp
                src/Parser.hpp
                src/Parsing/Camera.hpp
                src/Parsing/ParseHeader.hpp
                src/Parsing/ParseHits.hpp
                src/Parsing/ParseTrigger.hpp
                src/Parsing/Rotation.hpp
                src/Parsing/tpx3_helper.hpp
                src/QueueUtility/Extractor.hpp
                src/QueueUtility/Injector.hpp
                src/QueueUtility/Iterator.hpp
                src/QueueUtility/QueueConcepts.hpp
                src/QueueUtility/QueueMessage.hpp
                src/QueueUtility/Sequencer.hpp
                src/Utility/AdaptivBackoff.hpp
                src/Utility/MPMCQueue.h
                src/Utility/Memory_Map.hpp
                src/Utility/SPSCQueue.h
                src/Utility/bits.hpp
                src/Utility/helper.hpp
                src/Utility/optional.hpp
                src/clock_correction.hpp
)

target_include_directories(fast_tpx3_parser
        PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
                $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(fast_tpx3_parser PUBLIC cxx_std_26)
target_link_libraries(fast_tpx3_parser PUBLIC libassert::assert)
if(NOT WIN32)
        target_link_libraries(fast_tpx3_parser PRIVATE atomic)
endif()

if(MSVC)
        target_compile_options(fast_tpx3_parser PRIVATE /W4 /WX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(fast_tpx3_parser PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
        )
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
                target_compile_options(fast_tpx3_parser PRIVATE
                        -Wno-interference-size
                        -Wno-deprecated-declarations
                )
        endif()
endif()

install(TARGETS fast_tpx3_parser
        EXPORT fast_tpx3_parserTargets
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(EXPORT fast_tpx3_parserTargets
        FILE fast_tpx3_parserTargets.cmake
        NAMESPACE fast_tpx3_parser::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/fast_tpx3_parser"
)

configure_package_config_file(
        cmake/fast_tpx3_parserConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/fast_tpx3_parserConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/fast_tpx3_parser"
)
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/fast_tpx3_parserConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion
)
install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/fast_tpx3_parserConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/fast_tpx3_parserConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/fast_tpx3_parser"
)

option(BUILD_TESTING "Build unit tests" "${PROJECT_IS_TOP_LEVEL}")
if(BUILD_TESTING)
        include(CTest)
        find_package(GTest REQUIRED)
        find_package(Catch2 3 REQUIRED)

        add_executable(unit_tests
                src/Batch/Batch_Pool_test.cpp
                src/QueueUtility/Extractor_test.cpp
                src/QueueUtility/Injector_test.cpp
                src/QueueUtility/Sequencer_test.cpp
                src/Batch/Batch_test.cpp
                src/Operations/Sink_test.cpp
                src/Parsing/Camera_test.cpp
                src/Parsing/ParseHeader_test.cpp
                src/Parsing/ParseHits_test.cpp
                src/Parsing/ParseTrigger_test.cpp
                src/Parsing/tpx3_helper_test.cpp
                src/QueueUtility/Iterator_test.cpp
                src/Utility/AdaptivBackoff_test.cpp
                src/Utility/Memory_Map_test.cpp
                src/Utility/bits_test.cpp
                src/Utility/helper_test.cpp
                src/Utility/optional_test.cpp
                src/clock_correction_test.cpp
                src/Parsing/Rotation_test.cpp
                src/Operations/Sorter_test.cpp
                src/Operations/Worker_test.cpp
                src/Operations/Pipeline_test.cpp
        )
        target_link_libraries(unit_tests
                PRIVATE
                        fast_tpx3_parser
                        Catch2::Catch2WithMain
                        GTest::gtest
        )

        include(Catch)
        catch_discover_tests(unit_tests)
endif()

option(FAST_TPX3_PARSER_BUILD_TOOLS "Build the parser executable and benchmarks"
       "${PROJECT_IS_TOP_LEVEL}")
if(FAST_TPX3_PARSER_BUILD_TOOLS)
        find_package(benchmark REQUIRED)

        add_executable(benchmarks
                benchmarks/benchmark.cpp
                benchmarks/bm_clock_correction.cpp
                benchmarks/bm_parse_trigger.cpp
                benchmarks/bm_sorter.cpp
                benchmarks/bm_sink.cpp
        )
        target_link_libraries(benchmarks
                PRIVATE
                        fast_tpx3_parser
                        benchmark::benchmark
        )

        add_executable(main src/main.cpp)
        target_link_libraries(main PRIVATE fast_tpx3_parser)
endif()
