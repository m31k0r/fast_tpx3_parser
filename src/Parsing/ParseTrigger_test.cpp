#include <catch2/catch_test_macros.hpp>

#include "ParseTrigger.hpp"

TEST_CASE("Trigger parser preserves batch sequence and event data") {
    Batch_Pool<U64Batch> raw_pool(1);
    Batch_Pool<TriggerBatch> trigger_pool(1);
    auto raw = raw_pool.acquire_batch_blocking();
    raw->sequence_number = 17;
    raw->push_back((uint64_t{TRIGGER_NIBBLE} << 60) | (uint64_t{0xF} << 56) |
                   (uint64_t{4} << 9) | (uint64_t{1} << 5));

    ParseTrigger parser(trigger_pool);
    auto parsed = parser.parse(raw);
    REQUIRE(parsed->sequence_number == 17);
    REQUIRE(parsed->size == 1);
    REQUIRE(parsed->type[0] == 0xF);
    REQUIRE(parsed->toa[0] == 4 * 12 * 260 + 4 * 12 * 5 / 12);
}
