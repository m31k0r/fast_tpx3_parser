#include <catch2/catch_test_macros.hpp>

#include <array>

#include "tpx3_helper.hpp"

namespace {
uint64_t make_hit_word(uint16_t pixel_address, uint16_t toa = 20,
                       uint16_t tot = 511, uint8_t f_toa = 3,
                       uint16_t spidr = 1) {
    return (uint64_t{HIT_NIBBLE} << 60) |
           (uint64_t{pixel_address} << 44) | (uint64_t{toa} << 30) |
           (uint64_t{tot} << 20) | (uint64_t{f_toa} << 16) | spidr;
}

uint16_t make_pixel_address(uint16_t dcol, uint16_t spix, uint8_t pixel) {
    return static_cast<uint16_t>((dcol << 8) | (spix << 1) | pixel);
}
}  // namespace

TEST_CASE("TPX3 helpers decode hits, triggers, frames, and headers") {
    const auto hit_word = make_hit_word(make_pixel_address(2, 6, 3));
    const tpx3_helper::Hit hit(hit_word);
    const auto count = ((((uint64_t{1} << 14) + 20) << 4) - 3);
    const auto expected_toa = count * 1562 + count / 2;
    REQUIRE(hit.get_toa() == expected_toa);
    REQUIRE(hit.tot == 511);
    REQUIRE(hit.tot_cluster == 511);
    REQUIRE(hit.x == 2);
    REQUIRE(hit.y == 9);
    hit.set_toa(123);
    hit.set_delay(7);
    REQUIRE(hit.get_toa() == 123);
    REQUIRE(hit.get_delay() == 7);

    const uint64_t trigger_word = (uint64_t{TRIGGER_NIBBLE} << 60) |
                                 (uint64_t{0xF} << 56) | (uint64_t{2} << 9) |
                                 (uint64_t{3} << 5);
    const auto [type, toa] = tpx3_helper::parse_trigger(trigger_word);
    const uint64_t trigger_count = 2 * 12 + 3 - 1;
    REQUIRE(type == 0xF);
    REQUIRE(toa == trigger_count * 260 + trigger_count * 5 / 12);
    REQUIRE(tpx3_helper::parse_frame(1) ==
            (uint64_t{1} << 18) * 1562 + (uint64_t{1} << 18) / 2);
    REQUIRE(tpx3_helper::trigger_type_to_string(0xF) == "TDC1 Rise");
    REQUIRE(tpx3_helper::trigger_type_to_string(0xA) == "TDC1 Fall");
    REQUIRE(tpx3_helper::trigger_type_to_string(0xE) == "TDC2 Rise");
    REQUIRE(tpx3_helper::trigger_type_to_string(0xB) == "TDC2 Fall");
    REQUIRE(tpx3_helper::trigger_type_to_string(0) == "Unknown Trigger Type");

    const uint64_t header_word = uint64_t{'T'} | (uint64_t{'P'} << 8) |
                                 (uint64_t{'X'} << 16) |
                                 (uint64_t{'3'} << 24) | (uint64_t{2} << 32) |
                                 (uint64_t{16} << 48);
    const std::array<uint64_t, 3> chunk{header_word, hit_word, trigger_word};
    REQUIRE(tpx3_helper::check_health(chunk.data()));
    const auto header = tpx3_helper::parse_header(chunk.data());
    REQUIRE(header);
    REQUIRE(header->start == chunk.data());
    REQUIRE(header->end == chunk.data() + chunk.size());
    REQUIRE(header->chip_nr == 2);
}
