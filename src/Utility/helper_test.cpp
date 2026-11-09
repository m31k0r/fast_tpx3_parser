#include <catch2/catch_test_macros.hpp>

#include <array>
#include <utility>

#include "helper.hpp"

TEST_CASE("Array helpers initialize repeated and indexed elements") {
    const auto values = make_array<int>(std::make_index_sequence<3>{}, 7);
    REQUIRE((values == std::array<int, 3>{7, 7, 7}));
    const auto generated = make_array_l<int>(std::make_index_sequence<3>{},
                                             [](auto i) { return i; });
    REQUIRE((generated == std::array<int, 3>{0, 1, 2}));
}
