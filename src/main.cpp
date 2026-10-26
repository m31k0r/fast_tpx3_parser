#include <ctime>
#include <execution>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include "Memory_Map.hpp"
#include "Parser.hpp"
#include "clock_correction.hpp"
#include "tpx3_helper.hpp"

class MaxDisplacement {
   public:
    using Timestamp = long long;

    explicit MaxDisplacement(std::size_t k) : k_(k) {}

    void add(Timestamp timestamp) {
        buffer_.push({timestamp, nextIndex_++});

        // A k-displaced stream can release the smallest
        // timestamp once k+1 elements are buffered.
        while (buffer_.size() > k_) {
            processNext();
        }
    }

    auto get() -> size_t {
        while (!buffer_.empty()) {
            processNext();
        }
        if (maxDisplacement_ == k_)
            std::cerr << "MaxDis limit reached" << std::endl;
        return maxDisplacement_;
    }

   private:
    std::size_t k_;
    std::size_t nextIndex_ = 0;
    std::size_t sortedIndex_ = 0;
    std::size_t maxDisplacement_ = 0;

    // Sort by timestamp, then original index for duplicates.
    using Entry = std::pair<Timestamp, std::size_t>;

    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry> >
        buffer_;

    void processNext() {
        const auto [timestamp, originalIndex] = buffer_.top();
        buffer_.pop();

        const std::size_t displacement = originalIndex > sortedIndex_
                                             ? originalIndex - sortedIndex_
                                             : sortedIndex_ - originalIndex;

        maxDisplacement_ = std::max(maxDisplacement_, displacement);

        ++sortedIndex_;
    }
};

std::array<ClockCorrection<TWENTY_SIXISH_SECONDS>, 4> clock_correction;
ClockCorrection<ONE_HUNDRED_SECONDS> trigger_correction;
uint64_t trigger_before = 0;

std::array<uint64_t, 4> hit_before = {0, 0, 0, 0};
std::array<uint64_t, 4> delta_t = {0, 0, 0, 0};
std::array<MaxDisplacement, 4> max_dis{
    MaxDisplacement(300), MaxDisplacement(300), MaxDisplacement(300),
    MaxDisplacement(300)};

std::array<std::map<uint8_t, uint64_t>, 4> triggers;
std::array<uint64_t, 4> hits;
void parse_header(tpx3_helper::Header t_header) {
    for (const auto *data_ptr = t_header.start; data_ptr < t_header.end;
         data_ptr = std::next(data_ptr)) {
        const uint64_t data_entry = *data_ptr;
        auto nibble = to_nibble(data_entry);

        switch (nibble) {
            case HIT_NIBBLE: {
                auto hit = tpx3_helper::Hit(data_entry);
                const auto time =
                    clock_correction[t_header.chip_nr].clock(hit.get_toa());
                if (hit_before[t_header.chip_nr] > time) {
                    delta_t[t_header.chip_nr] =
                        std::max(delta_t[t_header.chip_nr],
                                 hit_before[t_header.chip_nr] - time);
                }
                hit_before[t_header.chip_nr] = time;
                max_dis[t_header.chip_nr].add(time);
                hits[t_header.chip_nr]++;
                break;
            }
            case TRIGGER_NIBBLE: {
                auto trigger = tpx3_helper::Trigger(data_entry);
                triggers[t_header.chip_nr][trigger.get_type()]++;
                const auto time = trigger_correction.clock(trigger.get_toa());
                if (trigger_before > time) {
                    std::cout << "Found disorder: " << trigger_before << " > "
                              << time << std::endl;
                }
                trigger_before = time;
                break;
            }
            case FRAME_NIBBLE: {
            }
            default:
                break;
        }
    }
    // std::cout << "Max delta t hits:" << delta_t << std::endl;
    // std::cout << (int)t_header.chip_nr << " " << earliest_time << " "
    //           << latest_time << "\n";
};
int parse(MemoryMap &t_mmap) {
    const auto *running = t_mmap.begin();
    while (std::next(running) < t_mmap.end()) {
        auto opt = tpx3_helper::parse_header(running);
        if (not opt) {
            return -1;
        }
        auto header = opt.value();
        running = header.end;
        if (header.end > t_mmap.end()) {
            return -1;
        }
        parse_header(header);
    }
    for (int i = 0; i < 4; ++i) {
        if (max_dis[i].get() > 0)
            std::cout << "Hits disorder @" << i << ": " << max_dis[i].get()
                      << " t: " << delta_t[i] / 1000 / 1000 << "ns"
                      << std::endl;
    }

    std::cout << "Number hits: ";
    uint64_t sum = 0;
    for (int i = 0; i < 4; ++i) {
        std::cout << hits[i] << " ";
        sum += hits[i];
    }
    std::cout << " = " << sum << std::endl;

    for (int i = 0; i < 4; ++i) {
        std::cout << "Chip: " << i << std::endl;
        for (const auto &[key, value] : triggers[i]) {
            std::cout << tpx3_helper::trigger_type_to_string(key) << ": "
                      << value << '\n';
        }
    }
    return 0;
};
int main(int argc, char *argv[]) {
    std::string filename = "tpx3.tpx3";
    if (argc == 2) filename = argv[1];
    MemoryMap mmap(filename);
    // parse(mmap);
    Parser<2> parser(std::move(mmap));
    std::cout << "Starting parsing" << std::endl;
    parser.parse();
    return 0;
}
