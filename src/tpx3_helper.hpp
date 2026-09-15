#ifndef TPX3_HELPER
#define TPX3_HELPER
#include <sys/types.h>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
struct Trigger {
    uint64_t toa{};
    uint64_t delay = 0;
    uint8_t type{};
    Trigger(uint64_t t_data);
    Trigger() = default;

   private:
    std::string to_string() const;
    [[nodiscard]] uint64_t get_toa() const;
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
struct TriggerEval {
    std::vector<uint64_t> toas;
    void merge(const TriggerEval &t_other);
    void clear();
};
struct TriggerSoA {
    std::vector<uint64_t> toas;
    std::vector<uint16_t> types;
    void push_back(const Trigger &t_trigger) {
        toas.push_back(t_trigger.toa);
        types.push_back(t_trigger.type);
    }
    void push_back(std::uint64_t t_toa, std::uint8_t t_type) {
        toas.push_back(t_toa);
        types.push_back(t_type);
    }
    [[nodiscard]] std::size_t num_triggers() const { return toas.size(); }
    [[nodiscard]] bool empty() const { return toas.empty(); }
    [[nodiscard]] Trigger at(std::size_t t_idx) const {
        Trigger trigger;
        trigger.toa = toas[t_idx];
        trigger.type = types[t_idx];
        return trigger;
    }
    void reserve(std::size_t t_size) {
        toas.reserve(t_size);
        types.reserve(t_size);
    }
    void clear() {
        toas.clear();
        types.clear();
    }
    void append(const TriggerSoA &t_triggers) {
        toas.insert(toas.end(), t_triggers.toas.begin(), t_triggers.toas.end());
        types.insert(types.end(), t_triggers.types.begin(),
                     t_triggers.types.end());
    }
    size_t size() const { return toas.size(); };
    using iter_toas_t = std::vector<uint64_t>::iterator;
    using iter_types_t = std::vector<uint16_t>::iterator;
    TriggerSoA() = default;
    TriggerSoA(iter_toas_t t_toas_it_begin, iter_toas_t t_toas_it_end,
               iter_types_t t_types_it_begin, iter_types_t t_types_it_end)
        : toas(t_toas_it_begin, t_toas_it_end),
          types(t_types_it_begin, t_types_it_end) {}
};

// Struct of Arrays layout for a batch of hits.
struct HitSoA {
    std::vector<uint64_t> toa;
    std::vector<uint64_t> delay;
    std::vector<uint32_t> tot;
    std::vector<uint32_t> tot_cluster;
    std::vector<uint16_t> x;
    std::vector<uint16_t> y;
    std::vector<uint8_t> size;

    [[nodiscard]] std::size_t num_hits() const { return toa.size(); }
    [[nodiscard]] bool empty() const { return toa.empty(); }

    void push_back(const Hit &t_hit) {
        toa.push_back(t_hit.toa);
        delay.push_back(t_hit.delay);
        tot.push_back(t_hit.tot);
        tot_cluster.push_back(t_hit.tot_cluster);
        x.push_back(t_hit.x);
        y.push_back(t_hit.y);
        size.push_back(t_hit.size);
    }

    [[nodiscard]] Hit at(std::size_t t_idx) const {
        Hit hit;
        hit.toa = toa[t_idx];
        hit.delay = delay[t_idx];
        hit.tot = tot[t_idx];
        hit.tot_cluster = tot_cluster[t_idx];
        hit.x = x[t_idx];
        hit.y = y[t_idx];
        hit.size = size[t_idx];
        return hit;
    }

    void reserve(std::size_t t_size) {
        toa.reserve(t_size);
        delay.reserve(t_size);
        tot.reserve(t_size);
        tot_cluster.reserve(t_size);
        x.reserve(t_size);
        y.reserve(t_size);
        size.reserve(t_size);
    }

    void clear() {
        toa.clear();
        delay.clear();
        tot.clear();
        tot_cluster.clear();
        x.clear();
        y.clear();
        size.clear();
    }
};

uint8_t get_byte(byte_t t_data, int t_byte);

constexpr auto get_bits(const byte_t t_data, const int t_start_bit,
                        const int t_end_bit) -> uint64_t {
    auto num = (t_end_bit - t_start_bit) + 1;
    uint64_t const mask = (uint64_t(1) << num) - 1;
    auto mask_shifted = mask << t_start_bit;
    return (t_data & mask_shifted) >> t_start_bit;
}
bool check_health(const byte_t *t_data);
std::optional<Header> parse_header(const byte_t *t_data_ptr);
uint64_t parse_frame(byte_t t_frame);
std::string trigger_type_to_string(uint8_t t_type);
}  // namespace tpx3_helper
#endif
