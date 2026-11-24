#include <catch2/catch_test_macros.hpp>

#include "Batch.hpp"
TEST_CASE("Batch ranges expose only populated entries") {
    U64Batch raw;
    raw.push_back(4);
    raw.push_back(9);
    REQUIRE(*raw.begin() == 4);
    REQUIRE(raw.end() - raw.begin() == 2);
    REQUIRE(std::distance(raw.cbegin(), raw.cend()) == 2);

}
