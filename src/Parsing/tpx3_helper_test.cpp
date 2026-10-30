
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <stdexcept>

#include "tpx3_helper.hpp"

using namespace tpx3_helper;
namespace {
constexpr uint64_t header_word(uint8_t chip, uint16_t size_bytes) {
    return uint64_t{'T'} | (uint64_t{'P'} << 8) | (uint64_t{'X'} << 16) |
           (uint64_t{'3'} << 24) | (uint64_t{chip} << 32) |
           (uint64_t{size_bytes} << 48);
}
}  // namespace

TEST_CASE("parse_trigger", "[tpx3_helper]") {
    // type in bits 56-59, timestep in 9-43, stamp in 5-8
    const uint64_t word = (uint64_t{0xf} << 56) | (uint64_t{1000} << 9) | (3 << 5);
    const auto [type, toa] = parse_trigger(word);
    const uint64_t count = 1000 * 12 + 3 - 1;
    REQUIRE(type == 0xf);
    REQUIRE(toa == count * 260 + count * 5 / 12);
}

TEST_CASE("parse_header", "[tpx3_helper]") {
    std::vector<byte_t> data(10, 0);
    data[0] = header_word(2, 8 * 4);

    SECTION("valid header") {
        const auto header = parse_header(data.data());
        REQUIRE(header.has_value());
        REQUIRE(header->chip_nr == 2);
        REQUIRE(header->start == data.data());
        REQUIRE(header->end == data.data() + 1 + 4);
    }

    SECTION("chunk size uses both size bytes") {
        data.assign(100, 0);
        data[0] = header_word(0, 0x0108);  // 264 bytes -> 33 words
        const auto header = parse_header(data.data());
        REQUIRE(header.has_value());
        REQUIRE(header->end == data.data() + 1 + 33);
    }
}

TEST_CASE("check_health accepts the TPX3 magic", "[tpx3_helper]") {
    const byte_t word = header_word(0, 0);
    REQUIRE(check_health(&word));
}

TEST_CASE("Hit pixel coordinates", "[tpx3_helper]") {
    // dcol=2 (0x200), superpixel bits 0x8 -> 4, pixel 5 -> x+1, y+1
    const uint64_t pix_addr = 0x200 | 0x8 | 0x5;
    const Hit hit(pix_addr << 44);
    REQUIRE(hit.x == 3);
    REQUIRE(hit.y == 5);
    REQUIRE(hit.tot == 0);
    REQUIRE(hit.tot_cluster == hit.tot);
    REQUIRE(hit.size == 1);
}

TEST_CASE("Hit timing and tot", "[tpx3_helper]") {
    const uint64_t spidr = 1;
    const uint64_t toa = 2;
    const uint64_t tot = 300;
    const uint64_t word = (tot << 20) | (toa << 30) | spidr;
    const Hit hit(word);
    const uint64_t count = ((spidr << 14) + toa) << 4;
    REQUIRE(hit.tot == 300);
    REQUIRE(hit.tot_cluster == 300);
    REQUIRE(hit.toa == count * 1562 + count / 2);
}

TEST_CASE("Hit fine toa is subtracted", "[tpx3_helper]") {
    const Hit base(uint64_t{1} << 30);
    const Hit fine((uint64_t{1} << 30) | (uint64_t{3} << 16));
    REQUIRE(base.toa - fine.toa > 0);
}

TEST_CASE("parse_frame", "[tpx3_helper]") {
    REQUIRE(parse_frame(0) == 0);
    const uint64_t coarse = uint64_t{5} << 18;
    REQUIRE(parse_frame(5) == coarse * 1562 + coarse / 2);
    // only the low 16 bits are used
    REQUIRE(parse_frame(0xABCD0005) == parse_frame(5));
}

TEST_CASE("trigger_type_to_string", "[tpx3_helper]") {
    REQUIRE(trigger_type_to_string(0xf) == "TDC1 Rise");
    REQUIRE(trigger_type_to_string(0xa) == "TDC1 Fall");
    REQUIRE(trigger_type_to_string(0xe) == "TDC2 Rise");
    REQUIRE(trigger_type_to_string(0xb) == "TDC2 Fall");
    REQUIRE(trigger_type_to_string(0x0) == "Unknown Trigger Type");
}
