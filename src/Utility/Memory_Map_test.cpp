#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "Memory_Map.hpp"

namespace {
auto temporary_path(const char* prefix) {
    return std::filesystem::temp_directory_path() /
           (std::string(prefix) +
            std::to_string(std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count()) +
            ".bin");
}
}  // namespace

TEST_CASE("Memory map reads words and transfers ownership") {
    const auto filename = temporary_path("fast_tpx3_parser_");
    const std::array<uint64_t, 2> data{0x1234, 0xFEDC};
    {
        std::ofstream output(filename, std::ios::binary);
        REQUIRE(output.good());
        output.write(reinterpret_cast<const char*>(data.data()), sizeof(data));
        REQUIRE(output.good());
    }
    {
        MemoryMap mapped(filename.string());
        REQUIRE(mapped.get_status());
        REQUIRE(mapped.data()[0] == data[0]);
        REQUIRE(mapped.begin() + 2 == mapped.end());
        MemoryMap moved(std::move(mapped));
        REQUIRE(moved.data()[1] == data[1]);
        REQUIRE(moved.begin() + 2 == moved.end());
    }
    std::filesystem::remove(filename);
}

TEST_CASE("Memory map reports a missing file") {
    const auto missing = temporary_path("fast_tpx3_parser_missing_");
    REQUIRE_THROWS_AS(MemoryMap(missing.string()),
                      std::filesystem::filesystem_error);
}
