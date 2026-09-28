#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include "tpx3_helper.hpp"

template <typename T, std::size_t Capacity, typename Compare = std::less<T>>
class SortingBuffer {
   public:
    std::optional<T> pushpop(T value) {
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

            return ret;
        }

        for (std::size_t i = size_; i > static_cast<std::size_t>(pos); --i)
            m_buffer[i] = m_buffer[i - 1];

        m_buffer[pos] = {value, toa};
        ++size_;

        return std::nullopt;
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

struct Compare_Hits {
    bool operator()(const uint64_t &a_toa, const uint64_t &b_toa) const {
        int32_t d = (int32_t)(((b_toa - a_toa) & 0x3FFF) << 18);
        return d < 0;
    }
    uint64_t get_toa(const uint64_t &data) const {
        return tpx3_helper::get_bits(data, 30, 43);
    }
};
struct Compare_Triggers {
    bool operator()(const uint64_t &a_toa, const uint64_t &b_toa) const {
        int32_t d = (int32_t)(((b_toa - a_toa) & 0x3FFF) << 34);
        return d < 0;
    }
    uint64_t get_toa(const uint64_t &data) const {
        return tpx3_helper::get_bits(data, 9, 43);
    }
};
