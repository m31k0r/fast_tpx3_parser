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
    ParseHeader(Pusher &batch_handle) : m_pusher(batch_handle) {}
};
