#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include "optional.hpp"

template <typename T, std::size_t Capacity, typename Compare = std::less<T>>
class SortingBuffer {
   public:
    stdx::optional<T> pushpop(T value) {
        static_assert(Capacity > 1, "Capacity must be atleast 2");
        const auto toa = comp.get_toa(value);

        int pos = static_cast<int>(std::min(size_, Capacity)) - 1;

        for (; pos >= 0; --pos) {
            if (!comp(toa, m_buffer[pos].toa)) break;
        }

        ++pos;

        if (size_ >= Capacity) {
            T ret = m_buffer[0].value;

            for (std::size_t i = 0; i < pos - 2; ++i)
                m_buffer[i] = m_buffer[i + 1];

            m_buffer[pos - 1] = {value, toa};

            return stdx::optional<T>{ret};
        }

        for (std::size_t i = size_; i > static_cast<std::size_t>(pos); --i)
            m_buffer[i] = m_buffer[i - 1];

        m_buffer[pos] = {value, toa};
        ++size_;

        return stdx::optional<T>{std::nullopt};
    }

    stdx::optional<T> pop() {
        if (size_ == 0) return stdx::optional<T>(std::nullopt);

        T ret = std::move(m_buffer[0].value);

        for (std::size_t i = 0; i + 1 < size_; ++i)
            m_buffer[i] = std::move(m_buffer[i + 1]);

        --size_;

        return stdx::optional<T>{ret};
    }

   private:
    struct timed {
        T value;
        uint64_t toa;
    };
    std::array<timed, Capacity> m_buffer{};
    std::size_t size_ = 0;
    Compare comp;
};
