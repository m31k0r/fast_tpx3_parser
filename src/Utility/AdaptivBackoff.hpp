#pragma once
#include <chrono>

class adaptive_backoff {
    using clock = std::chrono::steady_clock;

    static constexpr unsigned spin_limit = 64;
    static constexpr unsigned yield_limit = 256;

    static constexpr auto initial_sleep = std::chrono::microseconds(50);

    unsigned failures_ = 0;

    static void cpu_relax() noexcept;

   public:
    void pause() noexcept;

    void reset() noexcept { failures_ = 0; }
};
