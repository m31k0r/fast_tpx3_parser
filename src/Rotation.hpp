#pragma once
#include <cstdint>
#include <string>

struct Rotation {
    int8_t A{};
    int8_t B{};
    int8_t C{};
    int8_t D{};
    int8_t E{};
    int8_t F{};
    std::string name;
    [[nodiscard]] std::pair<uint16_t, uint16_t> rotate(
        const uint16_t t_x, const uint16_t t_y) const {
        constexpr size_t max_pix = 255;
        return {(A * max_pix) + (B * t_x) + (C * t_y),
                (D * max_pix) + (E * t_x) + (F * t_y)};
        // FIXME: Rotation is camera insensitive
    }
    static Rotation from_string(const std::string &t_orientation);
};
