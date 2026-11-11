#include <catch2/catch_test_macros.hpp>

#include "ParseHits.hpp"

namespace {
uint64_t make_hit_word(uint16_t pixel_address) {
    return (uint64_t{HIT_NIBBLE} << 60) | (uint64_t{pixel_address} << 44) |
           (uint64_t{20} << 30) | (uint64_t{511} << 20) | (uint64_t{3} << 16) |
           1;
}

uint16_t make_pixel_address(uint16_t dcol, uint16_t spix, uint8_t pixel) {
    return static_cast<uint16_t>((dcol << 8) | (spix << 1) | pixel);
}
}  // namespace

TEST_CASE("Hit parser fills batches and preserves decoded values") {
    Batch_Pool<HitBatch> pool(3);
    Batch_Pool<U64Batch> raw_pool(1);
    Camera camera;
    ParseHit parser(pool, camera, 0);
    const auto raw = make_hit_word(make_pixel_address(2, 6, 3));
    const auto decoded = tpx3_helper::Hit(raw);
    auto pre_parsed = parser.parse(raw);
    REQUIRE(pre_parsed.data == raw);
    REQUIRE(pre_parsed.toa == decoded.get_toa());

    auto input = raw_pool.acquire_batch_blocking();
    input->push_back(raw);

    auto no_toa = parser.parse_no_toa(&pre_parsed, &pre_parsed + 1);
    REQUIRE(no_toa->size == 1);
    REQUIRE(no_toa->tot[0] == 511);
    REQUIRE(no_toa->x[0] == 259);
    REQUIRE(no_toa->y[0] == 504);
}
