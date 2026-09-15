#include "Camera.hpp"
#include <cereal/archives/json.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "TOOLS/Vec2.hpp"
#include "TOOLS/logging.hpp"
#include "TPX3/tpx3_helper.hpp"
#include "config.hpp"
#include "fstream"

// constexpr size_t DEFAULT_NUM_PIXELS_CAM = 256;
// constexpr size_t DEFAULT_NUM_PIXELS = (DEFAULT_NUM_PIXELS_CAM * 2) + 2;
constexpr auto NUM = 256;
constexpr auto CH1_RISE = 0xF;
namespace camera {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
Vec2<size_t> m_number_pixels_cam = {NUM, NUM};
Vec2<size_t> m_number_pixels = {2 * NUM, 2 * NUM};
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

Vec2<size_t> number_pixels() { return m_number_pixels; };
Vec2<size_t> number_pixels_cam() { return m_number_pixels_cam; };
uint8_t number_of_chips() { return m_number_of_chips; };
uint16_t delay_trigger_type() { return m_delay_trigger_type; };
uint64_t trigger_delay() { return m_trigger_delay; };
void save_camera_geometry(const std::filesystem::path& t_filename) {
    auto file = std::ofstream(t_filename);
    if (!file.is_open()) {
        SPDLOG_ERROR("Failed to open camera geometry file for writing: {}",
                     t_filename.string());
        return;
    }
    cereal::JSONOutputArchive archive(file);
    archive(cereal::make_nvp("number_pixels", m_number_pixels));
    archive(cereal::make_nvp("number_pixels_cam", m_number_pixels_cam));
    archive(cereal::make_nvp("number_of_chips", m_number_of_chips));
    archive(cereal::make_nvp("shift_x", m_shift_x));
    archive(cereal::make_nvp("shift_y", m_shift_y));
    archive(cereal::make_nvp("rotation", m_rotation));
    archive(cereal::make_nvp("gap_fill", m_gap_fill));
    archive(cereal::make_nvp("row_correction", m_row_correction));
    archive(cereal::make_nvp("delay_trigger_type", m_delay_trigger_type));
    archive(cereal::make_nvp("trigger_delay", m_trigger_delay));
};
void load_camera_geometry(const std::filesystem::path& t_filename) {
    auto file = std::ifstream(t_filename);
    cereal::JSONInputArchive archive(file);
    try {
        archive(cereal::make_nvp("number_pixels", m_number_pixels));
        archive(cereal::make_nvp("number_pixels_cam", m_number_pixels_cam));
        archive(cereal::make_nvp("number_of_chips", m_number_of_chips));
        archive(cereal::make_nvp("shift_x", m_shift_x));
        archive(cereal::make_nvp("shift_y", m_shift_y));
        archive(cereal::make_nvp("rotation", m_rotation));
        archive(cereal::make_nvp("gap_fill", m_gap_fill));
        archive(cereal::make_nvp("row_correction", m_row_correction));
        archive(cereal::make_nvp("delay_trigger_type", m_delay_trigger_type));
        archive(cereal::make_nvp("trigger_delay", m_trigger_delay));
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error loading camera geometry from {}: {}",
                     t_filename.string(), e.what());
    }
};
void check_geometry_loaded() {
    // assert(shift_x.size() == number_of_chips);
    // assert(shift_y.size() == number_of_chips);
    // assert(rotation.size() == number_of_chips);
    // assert(row_correction.size() == number_of_chips);
    // assert(trigger_delay.size() == number_of_chips);
    // for (const auto& row_corr : m_row_correction) {
    //     assert(rc.size() == number_pixels_cam.y);
    // }
    // assert(gap_fill.size() == number_of_chips);
    // for (const auto& fill : m_gap_fill) {
    //     assert(fill <= 0b1111);
    // }
};
std::vector<std::filesystem::path> get_available_camera_geometries() {
#ifndef EMSCRIPTEN
    auto dir = config::get_config_dir() / "camera";
    std::vector<std::filesystem::path> files;
    for (const auto& file : std::filesystem::directory_iterator(dir)) {
        if (file.path().extension() == ".json") {
            files.push_back(file.path());
        }
    }
    return files;
#else
    return {};
#endif
};
}  // namespace camera
//

std::pair<uint16_t, uint16_t> camera::fill_gap(uint16_t t_x_chip,
                                               uint16_t t_y_chip,
                                               int t_chip_nr) {
    const auto gap = m_gap_fill.at(t_chip_nr);
    auto [x_rotated, y_rotated] =
        m_rotation[t_chip_nr].rotate(t_x_chip, t_y_chip);
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
        if (x_rotated == camera::number_pixels_cam().x - 2) {
            x_rotated += (rand() % 2);
        }
    }
    if ((gap & 0b1000) != 0) {
        if (y_rotated == camera::number_pixels_cam().x - 2) {
            y_rotated += (rand() % 2);
        }
    }
    return {x_rotated, y_rotated};
};

std::pair<uint16_t, uint16_t> camera::to_global(uint16_t t_x_chip,
                                                uint16_t t_y_chip,
                                                int t_chip_nr) {
    return {t_x_chip + m_shift_x[t_chip_nr], t_y_chip + m_shift_y[t_chip_nr]};
}

uint64_t camera::get_row_correction(size_t t_chip_nr, uint16_t t_row) {
    return m_row_correction[t_chip_nr][t_row];
}
