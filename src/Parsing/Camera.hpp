#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <utility>

namespace camera {
std::pair<size_t,size_t> number_pixels();
std::pair<size_t,size_t> number_pixels_cam();
uint8_t number_of_chips();
uint16_t delay_trigger_type();

std::pair<uint16_t, uint16_t> to_global(std::pair<uint16_t, uint16_t> pos_chip,
                                        uint8_t t_chip_nr);
std::pair<uint16_t, uint16_t> pixaddr(uint64_t t_encoded);
std::pair<uint16_t, uint16_t> fill_gap(std::pair<uint16_t, uint16_t> pos_chip,
                                       uint8_t t_chip_nr);
uint64_t get_row_correction(uint8_t t_chip_nr, uint16_t t_row);
uint64_t trigger_delay();
};  // namespace camera
