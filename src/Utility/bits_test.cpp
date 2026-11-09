#include <catch2/catch_test_macros.hpp>

#include "bits.hpp"

TEST_CASE("Bit helpers extract selected fields") {
    constexpr uint64_t word = 0xFEDC'BA98'7654'3210;
    REQUIRE(get_bits(word, 8, 15) == 0x32);
    REQUIRE(get_bits(word, 32, 47) == 0xBA98);
    REQUIRE(get_byte(word, 6) == 0xDC);
    REQUIRE(to_nibble(word) == 0xF);
}
