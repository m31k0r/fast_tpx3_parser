#pragma once
#include "cereal_fwd.hpp"
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <utility>
#include "TOOLS/Vec2.hpp"
namespace camera {

Vec2<size_t> number_pixels();
Vec2<size_t> number_pixels_cam();
uint8_t number_of_chips();
uint16_t delay_trigger_type();

std::pair<uint16_t, uint16_t> to_global(uint16_t t_x_chip, uint16_t t_y_chip,
                                        int t_chip_nr);
std::pair<uint16_t, uint16_t> pixaddr(uint64_t t_encoded);
std::pair<uint16_t, uint16_t> fill_gap(uint16_t t_x_chip, uint16_t t_y_chip,
                                       int t_chip_nr);
uint64_t get_row_correction(size_t t_chip_nr, uint16_t t_row);
uint64_t trigger_delay();

void save_camera_geometry(const std::filesystem::path& t_filename);
void load_camera_geometry(const std::filesystem::path& t_filename);
void check_geometry_loaded();
std::vector<std::filesystem::path> get_available_camera_geometries();
};  // namespace camera
