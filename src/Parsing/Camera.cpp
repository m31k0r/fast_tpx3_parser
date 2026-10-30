#include "Camera.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
#include "Parsing/tpx3_helper.hpp"
#include "Rotation.hpp"

// constexpr size_t DEFAULT_NUM_PIXELS_CAM = 256;
// constexpr size_t DEFAULT_NUM_PIXELS = (DEFAULT_NUM_PIXELS_CAM * 2) + 2;
constexpr auto NUM = 256;
constexpr auto CH1_RISE = 0xF;
namespace camera {
    // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
    std::pair<size_t,size_t> m_number_pixels_cam = {NUM, NUM};
    std::pair<size_t,size_t> m_number_pixels = {2 * NUM, 2 * NUM};
    std::uint8_t m_number_of_chips = 4;
    std::vector<std::uint32_t> m_shift_x = {NUM, NUM, 0, 0};
    std::vector<std::uint32_t> m_shift_y = {NUM, 0, 0, NUM};
    std::uint64_t m_trigger_delay = 0;
    std::vector<Rotation> m_rotation = {
        Rotation::from_string("LtRBtT"), Rotation::from_string("RtLTtB"),
        Rotation::from_string("RtLTtB"), Rotation::from_string("LtRBtT")};
    std::vector<std::vector<int64_t>> m_row_correction = {
        std::vector<int64_t>(NUM, 0), std::vector<int64_t>(NUM, 0),
        std::vector<int64_t>(NUM, 0), std::vector<int64_t>(NUM, 0)};
    std::vector<uint8_t> m_gap_fill{0b0000, 0b0000, 0b0000, 0b0000};
    uint16_t m_delay_trigger_type = CH1_RISE;
    // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

    std::pair<size_t,size_t> number_pixels() { return m_number_pixels; };
    std::pair<size_t,size_t> number_pixels_cam() { return m_number_pixels_cam; };
    uint8_t number_of_chips() { return m_number_of_chips; };
    uint16_t delay_trigger_type() { return m_delay_trigger_type; };
    uint64_t trigger_delay() { return m_trigger_delay; };

    std::pair<uint16_t, uint16_t> fill_gap(std::pair<uint16_t,uint16_t> pos_chip,
                                                   uint8_t t_chip_nr) {
        const auto gap = m_gap_fill.at(t_chip_nr);
        auto [x_rotated, y_rotated] =
            m_rotation[t_chip_nr].rotate(pos_chip.first, pos_chip.second);
        if ((gap & 0b1) != 0) {
            x_rotated++;
            if (x_rotated == 1) {
                x_rotated -= (rand() % 2);
            }
        }
        if ((gap & 0b10) != 0) {
            y_rotated++;
            if (y_rotated == 1) {
                y_rotated -= (rand() % 2);
            }
        }
        if ((gap & 0b100) != 0) {
            if (x_rotated == camera::number_pixels_cam().first - 2) {
                x_rotated += (rand() % 2);
            }
        }
        if ((gap & 0b1000) != 0) {
            if (y_rotated == camera::number_pixels_cam().first - 2) {
                y_rotated += (rand() % 2);
            }
        }
        return {
            static_cast<uint16_t>(x_rotated),
            static_cast<uint16_t>(y_rotated)
        };
    };

    std::pair<uint16_t, uint16_t> to_global(std::pair<uint16_t, uint16_t> pos_chip,
                                                    uint8_t t_chip_nr) {
        return {pos_chip.first + m_shift_x[t_chip_nr], pos_chip.second + m_shift_y[t_chip_nr]};
    }

    uint64_t get_row_correction(uint8_t t_chip_nr, uint16_t t_row) {
        return m_row_correction[t_chip_nr][t_row];
    }
};