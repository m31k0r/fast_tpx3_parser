#include <cstdint>
#include <vector>
#include "benchmark/benchmark.h"

#include "clock_correction.hpp"

constexpr uint64_t ROLL_OVER_TIME = 1000000;

enum class Pattern {
    Sequential,
    Backwards,
    Rollover
};

std::vector<uint64_t> makeInput(size_t count, Pattern pattern) {
    std::vector<uint64_t> result;
    result.reserve(count);

    uint64_t t = 1000000;

    for (size_t i = 0; i < count; ++i) {
        switch (pattern) {
            case Pattern::Sequential:
                t += 1000;
                break;

            case Pattern::Backwards:
                t += 1000;
                if (i % 10000 == 5000)
                    t -= 100000;
                break;

            case Pattern::Rollover:
                t += ROLL_OVER_TIME / 100000;
                t %= ROLL_OVER_TIME;
                break;
        }

        result.push_back(t);
    }

    return result;
}

// ------------------------------------------------------------
// Single clock benchmark
// ------------------------------------------------------------

template <Pattern P>
static void BM_Clock(benchmark::State& state) {
    constexpr size_t N = 4096;

    const auto input = makeInput(N, P);

    ClockCorrection<ROLL_OVER_TIME> clock;

    size_t index = 0;
    uint64_t result = 0;

    for (auto _ : state) {
        const uint64_t t = input[index];

        index = (index + 1) & (N - 1);

        result = clock.clock(t);

        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations());
}


BENCHMARK_TEMPLATE(BM_Clock, Pattern::Sequential)
    ->Name("Clock/Sequential");

BENCHMARK_TEMPLATE(BM_Clock, Pattern::Backwards)
    ->Name("Clock/Backwards");

BENCHMARK_TEMPLATE(BM_Clock, Pattern::Rollover)
    ->Name("Clock/Rollover");
BENCHMARK_MAIN();