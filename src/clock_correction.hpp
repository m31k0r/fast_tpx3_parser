
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
    uint64_t prevClock = 0;
    uint64_t extraTime = 0;
    bool upperHalf = false;
    uint64_t counter = 0;

    uint64_t clock(uint64_t t_clock, int t_chip_nr = -1);
};

template <uint64_t ROLL_OVER_TIME>
uint64_t ClockCorrection<ROLL_OVER_TIME>::clock(uint64_t t_clock,
                                                int t_chip_nr) {
    if (prevClock == 0) {
        prevClock = t_clock;
        return t_clock;
    }
    constexpr uint64_t half_roll_over_time = ROLL_OVER_TIME / 2;
    constexpr uint64_t quarter_roll_over_time = ROLL_OVER_TIME / 4;

    bool const half_passed =
        prevClock <= half_roll_over_time and
        (t_clock > half_roll_over_time);  // Passed 17.6s mark
                                          //
    bool const below_quarter =
        t_clock < quarter_roll_over_time;  // Quarter mark we define as ~8.8s
    bool const quarted_passed =
        prevClock <= quarter_roll_over_time and not below_quarter;

    if (half_passed and quarted_passed) {
        return t_clock + ((extraTime +
                           static_cast<uint64_t>(below_quarter and upperHalf)) *
                          ROLL_OVER_TIME);
    }

    // We get close to a rollover if we are in upperhalf of 26.8s time span.
    // If this is the case all TOA which are below quarter time must be a
    // result of rollover and get an extra 26.8s added.
    upperHalf |= half_passed;
    // If we pass the quarter mark we 'flush' the correction.
    // All hits from now on get the extra bits as we are unlikely to find a
    // hit from the previous 'cycle'.
    // extraTime += static_cast<uint64_t>(upperHalf and quarted_passed);
    if (upperHalf and quarted_passed) {
        upperHalf = false;
        extraTime += 1;
    }

    prevClock = t_clock;

    counter++;
    return t_clock +
           ((extraTime + static_cast<uint64_t>(below_quarter and upperHalf)) *
            ROLL_OVER_TIME);
}
