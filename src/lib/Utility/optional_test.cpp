#include <catch2/catch_test_macros.hpp>

#include "optional.hpp"

TEST_CASE("Tombstone optional supports value operations") {
    stdx::optional<int> value;
    REQUIRE_FALSE(value.has_value());
    REQUIRE(value.value_or(5) == 5);
    value = 12;
    REQUIRE(value);
    REQUIRE(*value == 12);
    REQUIRE(value.value() == 12);
    value.emplace(18);
    REQUIRE(*value == 18);
    value = std::nullopt;
    REQUIRE_FALSE(value);
    REQUIRE(value.value_or(3) == 3);
}
