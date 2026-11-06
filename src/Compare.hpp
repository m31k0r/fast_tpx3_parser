#pragma once
#include "Parsing/tpx3_helper.hpp"
#include "Utility/bits.hpp"
struct Compare_Hits {
    bool operator()(const uint64_t &a_toa, const uint64_t &b_toa) const {
        auto a = tpx3_helper::Hit(a_toa);
        auto b = tpx3_helper::Hit(b_toa);
        // int32_t d = (int32_t)(((b_toa - a_toa) & 0x3FFF) << 18);
        // return d < 0;
        return a.toa < b.toa;
    }
    uint64_t get_toa(const uint64_t &data) const {
        return get_bits(data, 30, 43);
    }
};
