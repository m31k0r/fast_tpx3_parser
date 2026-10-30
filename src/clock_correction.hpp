
#pragma once

#include <cstdint>
#include <iostream>

constexpr uint64_t TWENTY_SIXISH_SECONDS =
    (((1ULL << 34) - 1) * 1562) + (((1ULL << 34) - 1) / 2);
// 26843545575000ULL;
constexpr uint64_t ONE_HUNDRED_SECONDS =
    // (((1ULL << 36) - 1) * 1562) + (((1ULL << 36) - 1) / 2);
    107374182400520ULL;

template <uint64_t ROLL_OVER_TIME>
struct ClockCorrection {
    static constexpr uint64_t HALF = ROLL_OVER_TIME / 2;
    static constexpr uint64_t QUARTER = ROLL_OVER_TIME / 4;

    uint64_t prevClock = 0;
    uint64_t offset = 0;
    bool upperHalf = false;

    [[nodiscard]] inline uint64_t clock(uint64_t t) noexcept {
        if (prevClock == 0) [[unlikely]] {
            prevClock = t;
            return t;
        }

        const bool below = t < QUARTER;

        const bool halfPassed =
            prevClock <= HALF && t > HALF;

        const bool quarterPassed =
            prevClock <= QUARTER && !below;

        if (halfPassed && quarterPassed) {
            return t + offset +
                   ((below && upperHalf) ? ROLL_OVER_TIME : 0);
        }

        upperHalf |= halfPassed;

        if (upperHalf && quarterPassed) {
            upperHalf = false;
            offset += ROLL_OVER_TIME;
        }

        prevClock = t;

        return t + offset +
               ((below && upperHalf) ? ROLL_OVER_TIME : 0);
    }
};