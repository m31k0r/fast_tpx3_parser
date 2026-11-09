#include <catch2/catch_test_macros.hpp>

#include "Iterator.hpp"

TEST_CASE("Batch iterators expose hit, trigger, and word values") {
    HitBatch hits;
    hits.push_back({100, 8, 3, 4});
    HitIterator hit(&hits, 0);
    REQUIRE(static_cast<bool>(hit));
    REQUIRE(hit.toa() == 100);
    REQUIRE(hit.tot() == 8);
    REQUIRE(hit.x() == 3);
    REQUIRE(hit.y() == 4);

    TriggerBatch triggers;
    triggers.push_back(200, 15);
    TriggerIterator trigger(&triggers, 0);
    REQUIRE(trigger.toa() == 200);
    REQUIRE(trigger.type() == 15);

    U64Batch words;
    words.push_back(9);
    U64Iterator word(&words, 0);
    REQUIRE(static_cast<bool>(word));
    REQUIRE(*word == 9);
    REQUIRE_FALSE(static_cast<bool>(U64Iterator(nullptr, 0)));
}
