
#include "Rotation.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <stdexcept>
#include "gtest/gtest.h"

namespace {
TEST_CASE("Rotation from string") {
    std::array<std::string, 8> rots = {"LtRBtT", "TtBLtR", "RtLTtB", "BtTLtR",
                                       "RtLBtT", "TtBRtL", "LtRTtB", "BtTRtL"};
    for (auto r : rots) {
        auto rot = Rotation::from_string(r);
    };
    REQUIRE_THROWS_AS(Rotation::from_string("invalid"), std::runtime_error);
}

TEST_CASE("Rotation corner transformation") {
    constexpr uint16_t M = 255;

    const std::array<std::pair<uint16_t, uint16_t>, 4> corners = {{
        {0, 0},  // top-left
        {M, 0},  // top-right
        {M, M},  // bottom-right
        {0, M},  // bottom-left
    }};

    struct TestCase {
        std::string orientation;
        std::array<std::pair<uint16_t, uint16_t>, 4> expected;
    };
    const std::array<TestCase, 8> tests = {{
        {"LtRBtT",  // x, 255-y
         {{
             {0, M},  // TL -> BL
             {M, M},  // TR -> BR
             {M, 0},  // BR -> TR
             {0, 0},  // BL -> TL
         }}},
        {"TtBLtR",  // y, x
         {{
             {0, 0},  // TL -> TL
             {0, M},  // TR -> BL
             {M, M},  // BR -> BR
             {M, 0},  // BL -> TR
         }}},
        {"RtLTtB",  // 255-x, y
         {{
             {M, 0},  // TL -> TR
             {0, 0},  // TR -> TL
             {0, M},  // BR -> BL
             {M, M},  // BL -> BR
         }}},
        {"BtTLtR",  // y, 255-x
         {{
             {0, M},  // TL -> BL
             {0, 0},  // TR -> TL
             {M, 0},  // BR -> TR
             {M, M},  // BL -> BR
         }}},
        {"RtLBtT",  // 255-x, 255-y
         {{
             {M, M},  // TL -> BR
             {0, M},  // TR -> BL
             {0, 0},  // BR -> TL
             {M, 0},  // BL -> TR
         }}},
        {"TtBRtL",  // 255-y, x
         {{
             {M, 0},  // TL -> TR
             {M, M},  // TR -> BR
             {0, M},  // BR -> BL
             {0, 0},  // BL -> TL
         }}},
        {"LtRTtB",  // x, y
         {{
             {0, 0},  // TL -> TL
             {M, 0},  // TR -> TR
             {M, M},  // BR -> BR
             {0, M},  // BL -> BL
         }}},
        {"BtTRtL",  // 255-y, 255-x
         {{
             {M, M},  // TL -> BR
             {M, 0},  // TR -> TR
             {0, 0},  // BR -> TL
             {0, M},  // BL -> BL
         }}},
    }};

    for (const auto& test : tests) {
        const auto rotation = Rotation::from_string(test.orientation);

        CAPTURE("rotation: " + test.orientation);

        for (size_t i = 0; i < corners.size(); ++i) {
            const auto [x, y] = corners[i];
            const auto actual = rotation.rotate(x, y);

            REQUIRE(actual == test.expected[i]);
        }
    }
}
}  // namespace
