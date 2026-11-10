#pragma once
#include <bits/error_constants.h>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <ostream>
#include <system_error>
#include <variant>

#include "Constants.hpp"
#include "QueueUtility/QueueMessage.hpp"
#include "Utility/Memory_Map.hpp"
#include "Utility/AdaptivBackoff.hpp"
#include "tpx3_helper.hpp"

using HeaderMessage = QueueMessage<tpx3_helper::Header>;

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
    // Consumes headers until EndOfFile is received.
    template <typename Queue>
    void parse(Queue &t_queue) {
        adaptive_backoff backoff;
        while (true) {
            auto *front = t_queue.front();
            if (front == nullptr) {
                backoff.pause();
                continue;
            }
            backoff.reset();
            const auto message = *front;
            t_queue.pop();
            if (std::holds_alternative<EndOfFile>(message)) {
                return;
            }
            parse_header(std::get<tpx3_helper::Header>(message));
        }
    }
    // Splits the mapped file into headers and feeds them to the queue,
    // always terminating the stream with EndOfFile.
    template <typename Queue>
    static std::error_code feed(MemoryMap &t_mmap, Queue &t_queue) {
        const auto push = [&t_queue](auto message) {
            adaptive_backoff backoff;
            while (not t_queue.try_push(message)) {
                backoff.pause();
            }
        };
        std::error_code ec;
        const auto *running = t_mmap.begin();
        while (std::next(running) < t_mmap.end()) {
            auto opt = tpx3_helper::parse_header(running);
            if (not opt) {
                ec = std::make_error_code(std::errc::io_error);
                break;
            }
            auto header = opt.value();
            running = header.end;
            if (header.end > t_mmap.end()) {
                ec = std::make_error_code(std::errc::invalid_seek);
                break;
            }
            push(header);
        }
        push(EndOfFile{});
        return ec;
    }
    ParseHeader(Pusher &batch_handle) : m_pusher(batch_handle) {}
};
