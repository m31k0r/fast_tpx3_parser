#include <catch2/catch_test_macros.hpp>

#include "Camera.hpp"

TEST_CASE("Camera transforms chip coordinates into camera coordinates") {
    Camera camera;
    REQUIRE((camera.to_global({10, 20}, 0) ==
             std::pair<uint16_t, uint16_t>{266, 276}));
    REQUIRE((camera.to_global({10, 20}, 1) ==
             std::pair<uint16_t, uint16_t>{266, 20}));
    REQUIRE((camera.fill_gap({10, 20}, 0) ==
             std::pair<uint16_t, uint16_t>{10, 235}));
    REQUIRE(camera.get_row_correction(0, 10) == 0);
}
