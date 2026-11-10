#include <catch2/catch_test_macros.hpp>

#include "clock_correction.hpp"

TEST_CASE("Clock correction preserves increasing timestamps") {
    ClockCorrection<1000> correction;
    REQUIRE(correction.clock(10) == 10);
    REQUIRE(correction.clock(20) == 20);
}
