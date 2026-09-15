#include "tpx3_helper.hpp"
#include <cstdint>
#include <iostream>
#include <optional>
#include "iso646.h"

namespace tpx3_helper {

inline bool healthy(uint64_t t_data) {
    return get_byte(t_data, 0) == 'T' and get_byte(t_data, 1) == 'P' and
           get_byte(t_data, 2) == 'X' and get_byte(t_data, 3) == '3';
}
bool check_health(const byte_t *t_data) {
    if (not healthy(*t_data)) {
        const auto *next_valid = t_data;
        while (true) {
            next_valid = std::next(next_valid);
            if (healthy(*next_valid)) {
                break;
            }
        }
        std::cerr << "Next valid header in" << next_valid - t_data << '\n';
        return false;
    }
    return true;
};

const byte_t *find_next_chunk(const byte_t *t_data) {
    if (not healthy(*t_data)) {
        std::cerr << "Wrong header" << '\n';
        const auto *next_valid = t_data;
        while (true) {
            next_valid = std::next(next_valid);
            if (healthy(*next_valid)) {
                break;
            }
        }
        std::cerr << "Next valid header in" << next_valid - t_data << '\n';
        return next_valid;
    }
    return t_data;
};

std::optional<Header> parse_header(const byte_t *t_data_ptr) {
    // const byte_t *header_ptr = find_next_chunk(data_ptr);
    const byte_t *header_ptr = t_data_ptr;
    const byte_t data = *header_ptr;
    assert(check_health(header_ptr));
    if (not check_health(header_ptr)) {
        return std::nullopt;
    }

    const uint8_t chip_nr = get_bits(data, 32, 39);

    // const char reserved = get_byte(data, 5);
    const long chunk_size = get_byte(data, 6) + (get_byte(data, 7) << 8);
    Header header{};
    header.start = t_data_ptr;
    t_data_ptr = std::next(t_data_ptr);
    header.end = std::next(t_data_ptr, (chunk_size / 8));
    header.chip_nr = chip_nr;
    /* printf("Header start: %p\n", header.start); */
    /* printf("Header end: %p\n", header.end); */
    /* printf("header start_content: %16x\n", *header.start); */
    /* printf("header end_content: %16x\n", *header.end); */
    return {header};
}
Hit::Hit(const uint64_t t_data) {
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    auto pix_addr = get_bits(t_data, 44, 59);
    auto toa = get_bits(t_data, 30, 43);
    auto tot = get_bits(t_data, 20, 29);
    auto f_toa = get_bits(t_data, 16, 19);
    auto spidr = get_bits(t_data, 0, 15);

    auto count = ((((static_cast<uint64_t>(spidr) << 14) + toa) << 4) - f_toa);
    constexpr uint64_t clock_factor = 1562;
    this->toa = (count * clock_factor) + (count / 2);
    this->tot = tot;
    this->tot_cluster = tot;

    auto dcol = (pix_addr & 0x0FE00) >> 8;
    // superpixel * 4
    auto spix = (pix_addr & 0x001F8) >> 1;  // (16+28+3-2)
    // pixel
    auto pix = pix_addr & 0x00007;
    auto x = (dcol + (pix / 4));
    auto y = (spix + (pix & 0x3));
    this->x = x;
    this->y = y;
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
Trigger::Trigger(const uint64_t t_data) : type(get_bits(t_data, 56, 59)) {
    /* trigger.timestep = get_bits(data, 9, 43); */
    auto timestep = get_bits(t_data, 9, 43);
    // trigger.counter = get_bits(t_data, 44, 55);
    auto stamp = get_bits(t_data, 5, 8);
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    // ps
    assert(stamp <= 12);
    assert(stamp >= 1);

    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    // reasoning: Use of magic numbers for bit manipulation
    auto count = ((timestep * 12) + stamp - 1);
    this->toa = (count * 260) + (count * 5 / 12);
    // static_cast<uint64_t>(
    //     (0.4166666666666 * static_cast<double>(count)));  // copied
}
std::string Trigger::to_string() const { return trigger_type_to_string(type); }
std::string trigger_type_to_string(uint8_t t_type) {
    switch (t_type) {
        case 0xf:
            return "TDC1 Rise";
        case 0xa:
            return "TDC1 Fall";
        case 0xe:
            return "TDC2 Rise";
        case 0xb:
            return "TDC2 Fall";
        default:
            return "Unknown Trigger Type";
    }
};
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

uint64_t parse_frame(const byte_t t_frame) {
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    // reasoning: Use of magic numbers for bit manipulation
    uint64_t coarse = (t_frame & 0xFFFF) << 18;
    uint64_t clock = (coarse * 1562) + (coarse / 2);
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    return clock;
};
void TriggerEval::merge(const TriggerEval &t_other) {
    toas.insert(toas.end(), t_other.toas.begin(), t_other.toas.end());
}
void TriggerEval::clear() { toas.clear(); }
uint8_t get_byte(const byte_t t_data, const int t_byte) {
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,
    // readability-magic-numbers)
    return (t_data >> (8 * t_byte)) & 0xff;
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,
    // readability-magic-numbers)
}
}  // namespace tpx3_helper
