#pragma once

#include <algorithm>
#include <assert.hpp>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <new>
#include <thread>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

constexpr auto num_runs = 30;
constexpr size_t BATCH_SIZE = std::hardware_constructive_interference_size * 16;
constexpr size_t BATCH_POOL_SIZE = 32 * 32;
constexpr size_t QUEUE_SIZE = 16;

class adaptive_backoff {
    using clock = std::chrono::steady_clock;

    static constexpr unsigned spin_limit = 64;
    static constexpr unsigned yield_limit = 256;

    static constexpr auto initial_sleep = std::chrono::microseconds(50);

    unsigned failures_ = 0;

    static void cpu_relax() noexcept {
#if defined(__x86_64__) || defined(__i386__)
        _mm_pause();
#elif defined(__aarch64__) || defined(__arm__)
        __asm__ __volatile__("yield");
#else
        std::this_thread::yield();
#endif
    }

   public:
    void pause() noexcept {
        using namespace std::chrono;

        if (failures_ < spin_limit) {
            cpu_relax();
        } else if (failures_ < yield_limit) {
            std::this_thread::yield();
        } else {
            const auto exponent = std::min((failures_ - yield_limit) / 32, 8u);

            const auto delay = initial_sleep * (1u << exponent);
            std::this_thread::sleep_for(delay);
        }

        if (failures_ < yield_limit + 160) ++failures_;

        // if (failures_ == yield_limit + 160) {
        //     std::cerr << "Queue stuck:\n"
        //               << libassert::stacktrace() << "\n\n"
        //               << std::endl;
        // }
    }

    void reset() noexcept { failures_ = 0; }
};
