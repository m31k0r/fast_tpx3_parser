#include <catch2/catch_test_macros.hpp>

#include "AdaptivBackoff.hpp"

TEST_CASE("Adaptive backoff supports repeated pauses and reset") {
    adaptive_backoff backoff;
    for (size_t i = 0; i < 400; ++i) backoff.pause();
    backoff.reset();
    backoff.pause();
    SUCCEED();
}
