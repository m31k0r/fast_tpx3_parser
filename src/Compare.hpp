#pragma once
#include "Utility/bits.hpp"
struct Compare_Hits {
    bool operator()(const uint64_t &a_toa, const uint64_t &b_toa) const {
        int32_t d = (int32_t)(((b_toa - a_toa) & 0x3FFF) << 18);
        return d < 0;
    }
    uint64_t get_toa(const uint64_t &data) const {
        return get_bits(data, 30, 43);
    }
};
struct Compare_Triggers {
    bool operator()(const uint64_t &a_toa, const uint64_t &b_toa) const {
        int32_t d = (int32_t)(((b_toa - a_toa) & 0x3FFF) << 34);
        return d < 0;
    }
    uint64_t get_toa(const uint64_t &data) const {
        return get_bits(data, 9, 43);
    }
};
