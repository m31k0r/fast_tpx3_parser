#pragma once
template<typename T>
constexpr auto get_bits(const T t_data, const int t_start_bit,
                        const int t_end_bit) -> T {
    auto num = (t_end_bit - t_start_bit) + 1;
    T const mask = (T(1) << num) - 1;
    auto mask_shifted = mask << t_start_bit;
    return (t_data & mask_shifted) >> t_start_bit;
}

template<typename T>
uint8_t get_byte(const T t_data, const int t_byte) {
    return (t_data >> (8 * t_byte)) & 0xff;
}