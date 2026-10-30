#ifndef TPX3_HELPER
#define TPX3_HELPER
#include <sys/types.h>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Utility/bits.hpp"

constexpr uint64_t FRAME_NIBBLE = 0x7;
constexpr uint64_t TRIGGER_NIBBLE = 0x6;
constexpr uint64_t HIT_NIBBLE = 0xb;

constexpr auto to_nibble(auto t_data_entry) {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    return t_data_entry >> 60;
}

namespace tpx3_helper {

enum class Status : std::uint8_t {
    OK,
    TRIGGER_ERROR,
    HEADER_ERROR,
    DATA_ERROR,
    FILE_ERROR,
    NO_FILE
};

using byte_t = uint64_t;
inline std::pair<uint8_t, uint64_t> parse_trigger(uint64_t t_data) {
    auto timestep = get_bits(t_data, 9, 43);
    // auto counter = get_bits(t_data, 44, 55);
    auto stamp = get_bits(t_data, 5, 8);
    // assert(stamp <= 12);
    // assert(stamp >= 1);
    auto count = ((timestep * 12) + stamp - 1);
    const auto toa = (count * 260) + (count * 5 / 12);
    const auto type = get_bits(t_data, 56, 59);
    return {type, toa};
};
struct Header {
   public:
    const byte_t *start;
    const byte_t *end;
    uint8_t chip_nr;
};
struct Hit {
    uint64_t toa{};
    uint64_t delay = 0;
    uint32_t tot{};
    uint32_t tot_cluster{};
    uint16_t x{};
    uint16_t y{};
    uint8_t size = 1;

    Hit() = default;
    Hit(uint64_t t_data);
    [[nodiscard]] uint64_t get_toa() const { return toa; }
    [[nodiscard]] uint64_t get_delay() const { return delay; }
    void set_delay(uint64_t t_delay) { delay = t_delay; }
    void set_toa(uint64_t t_toa) { toa = t_toa; }
};

bool check_health(const byte_t *t_data);
std::optional<Header> parse_header(const byte_t *t_data_ptr);
uint64_t parse_frame(byte_t t_frame);
std::string trigger_type_to_string(uint8_t t_type);
}  // namespace tpx3_helper
#endif
