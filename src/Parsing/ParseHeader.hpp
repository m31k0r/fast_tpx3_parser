#pragma once
#include <bits/error_constants.h>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <ostream>
#include <system_error>

#include "Constants.hpp"
#include "Utility/Memory_Map.hpp"
#include "tpx3_helper.hpp"

template <typename Pusher>
class ParseHeader {
    Pusher &m_pusher;

   public:
    void parse_header(tpx3_helper::Header t_header) {
        for (const auto *data_ptr = t_header.start; data_ptr < t_header.end;
             data_ptr = std::next(data_ptr)) {
            const uint64_t data_entry = *data_ptr;
            auto nibble = to_nibble(data_entry);

            switch (nibble) {
                case HIT_NIBBLE: {
                    m_pusher.insert_hit(data_entry, t_header.chip_nr);
                    break;
                }
                case TRIGGER_NIBBLE: {
                    m_pusher.insert_trigger(data_entry);
                    break;
                }
                case FRAME_NIBBLE: {
                }
                default:
                    break;
            }
        }
    };
    std::error_code parse(MemoryMap &t_mmap) {
        const auto *running = t_mmap.begin();
        // const auto total = std::distance(t_mmap.begin(), t_mmap.end());
        // auto last_progress = 0;
        while (std::next(running) < t_mmap.end()) {
            auto opt = tpx3_helper::parse_header(running);
            if (not opt) {
                return std::make_error_code(std::errc::io_error);
            }
            auto header = opt.value();
            running = header.end;
            if (header.end > t_mmap.end()) {
                return std::make_error_code(std::errc::invalid_seek);
            }
            parse_header(header);
            // const auto processed = running - t_mmap.begin();
            // const int progress =
            //     total > 0 ? static_cast<int>(100.0 * processed / total)
            //               : 100;
            //
            // if (progress != last_progress) {
            //     print_progress(processed, total);
            //     last_progress = progress;
            // }
        }
        return {};
    }
    ParseHeader(Pusher &batch_handle) : m_pusher(batch_handle) {}
};
