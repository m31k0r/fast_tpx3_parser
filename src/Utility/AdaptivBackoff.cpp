#include "AdaptivBackoff.hpp"
#include <xmmintrin.h>
#include <assert.hpp>
#include <iostream>
#include <stacktrace>
#include <thread>

void adaptive_backoff::cpu_relax() noexcept {
#if defined(__x86_64__) || defined(__i386__)
    _mm_pause();
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield");
#else
    std::this_thread::yield();
#endif
}

void adaptive_backoff::pause() noexcept {
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
    if (failures_ == yield_limit + 155)
        std::cout << libassert::stacktrace() << std::endl;
}
