#include <catch2/catch_test_macros.hpp>

#include "Compare.hpp"

TEST_CASE("Hit comparison orders raw hit words by timestamp") {
    const auto earlier = uint64_t{HIT_NIBBLE} << 60 | (uint64_t{1} << 30);
    const auto later = uint64_t{HIT_NIBBLE} << 60 | (uint64_t{2} << 30);
    Compare_Hits compare;
    REQUIRE(compare(earlier, later));
    REQUIRE_FALSE(compare(later, earlier));
    REQUIRE(compare.get_toa(earlier) == 1);
}
