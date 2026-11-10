#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

#include "ParseHeader.hpp"
#include "Utility/Memory_Map.hpp"

namespace {
struct RecordingPusher {
    std::vector<std::pair<uint64_t, uint8_t>> hits;
    std::vector<uint64_t> triggers;

    void insert_hit(uint64_t word, uint8_t chip) {
        hits.emplace_back(word, chip);
    }
    void insert_trigger(uint64_t word) { triggers.push_back(word); }
};

auto temporary_path() {
    return std::filesystem::temp_directory_path() /
           ("fast_tpx3_parser_header_" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()) +
            ".bin");
}
}  // namespace

TEST_CASE("Header parser routes hit and trigger words") {
    RecordingPusher pusher;
    ParseHeader parser(pusher);
    const uint64_t hit = (uint64_t{HIT_NIBBLE} << 60) | (uint64_t{1} << 44);
    const uint64_t trigger =
        (uint64_t{TRIGGER_NIBBLE} << 60) | (uint64_t{3} << 56);
    const std::array<uint64_t, 4> words{uint64_t{0x7} << 60, hit, trigger,
                                        uint64_t{FRAME_NIBBLE} << 60};

    parser.parse_header({words.data(), words.data() + words.size(), 3});

    REQUIRE(
        (pusher.hits == std::vector<std::pair<uint64_t, uint8_t>>{{hit, 3}}));
    REQUIRE((pusher.triggers == std::vector<uint64_t>{trigger}));
}

TEST_CASE(
    "Header parser processes mapped chunks and rejects oversized chunks") {
    const auto filename = temporary_path();
    const uint64_t header = uint64_t{'T'} | (uint64_t{'P'} << 8) |
                            (uint64_t{'X'} << 16) | (uint64_t{'3'} << 24) |
                            (uint64_t{1} << 32) | (uint64_t{16} << 48);
    const uint64_t hit = (uint64_t{HIT_NIBBLE} << 60) | (uint64_t{1} << 44);
    const uint64_t trigger =
        (uint64_t{TRIGGER_NIBBLE} << 60) | (uint64_t{3} << 56);
    const std::array<uint64_t, 4> valid_chunk{header, hit, trigger, 0};
    {
        std::ofstream output(filename, std::ios::binary);
        output.write(reinterpret_cast<const char*>(valid_chunk.data()),
                     sizeof(valid_chunk));
        REQUIRE(output.good());
    }
    {
        RecordingPusher pusher;
        ParseHeader parser(pusher);
        MemoryMap mapped(filename.string());
        REQUIRE_FALSE(parser.parse(mapped));
        REQUIRE(pusher.hits.size() == 1);
        REQUIRE((pusher.hits[0] == std::pair<uint64_t, uint8_t>{hit, 1}));
        REQUIRE((pusher.triggers == std::vector<uint64_t>{trigger}));
    }

    const uint64_t oversized_header =
        (header & ~(uint64_t{0xFFFF} << 48)) | (uint64_t{24} << 48);
    {
        std::ofstream output(filename, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(&oversized_header),
                     sizeof(oversized_header));
        output.write(reinterpret_cast<const char*>(&hit), sizeof(hit));
        REQUIRE(output.good());
    }
    {
        RecordingPusher pusher;
        ParseHeader parser(pusher);
        MemoryMap mapped(filename.string());
        REQUIRE(parser.parse(mapped) ==
                std::make_error_code(std::errc::invalid_seek));
        REQUIRE(pusher.hits.empty());
    }
    std::filesystem::remove(filename);
}
