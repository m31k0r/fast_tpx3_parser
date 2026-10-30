
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <stdexcept>

#include "../src/Parsing/tpx3_helper.hpp"

using namespace tpx3_helper;
TEST_CASE("Trigger") {
    Trigger trigger(0);

    // REQUIRE(trigger.get_toa() == 0);
    // REQUIRE(trigger.get_type() == 0);
    trigger.pack(3, 12345);

    REQUIRE(trigger.get_toa() == 12345);
    REQUIRE(trigger.get_type() == 3);
}
