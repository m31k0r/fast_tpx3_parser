#pragma once
#include <system_error>
#include "Batch.hpp"
#include "Batch_Pool.hpp"
#include "MPMCQueue.h"
#include "Memory_Map.hpp"
#include "Parser.hpp"
#include "tpx3_helper.hpp"

class ParseTrigger {
    Batch_Pool<TriggerBatch> &batch_pool;

   public:
    TriggerBatchPtr parse(U64BatchPtr batch) {
        TriggerBatchPtr out_batch = batch_pool.acquire_batch_blocking();
        out_batch->sequence_number = batch->sequence_number;
        for (auto u64 : *batch) {
            auto trigger = tpx3_helper::Trigger(u64);
            out_batch->push_back(trigger.get_toa(), 0, trigger.get_type());
        }
        return out_batch;
    }

    ParseTrigger(Batch_Pool<TriggerBatch> &t_batch_pool)
        : batch_pool(t_batch_pool) {}
};

class ParseHit {
    Batch_Pool<HitBatch> &batch_pool;

   public:
    HitBatchPtr parse(U64BatchPtr batch) {
        HitBatchPtr out_batch = batch_pool.acquire_batch_blocking();
        out_batch->sequence_number = batch->sequence_number;
        for (auto u64 : *batch) {
            auto hit = tpx3_helper::Hit(u64);
            out_batch->toa[out_batch->size] = hit.get_toa();
            out_batch->tot[out_batch->size] = hit.tot;
            out_batch->x[out_batch->size] = hit.x;
            out_batch->y[out_batch->size] = hit.y;
            out_batch->size++;
        }
        return out_batch;
    }

    ParseHit(Batch_Pool<HitBatch> &t_batch_pool) : batch_pool(t_batch_pool) {}
};

void print_progress(std::size_t processed, std::size_t total) {
    constexpr int width = 50;

    const int progress =
        total > 0 ? static_cast<int>(100.0 * processed / total) : 100;

    const int filled = progress * width / 100;

    std::cerr << "\rProgress: [";
    for (int i = 0; i < width; ++i) {
        std::cerr << (i < filled ? '#' : '-');
    }

    std::cerr << "] " << progress << '%' << std::flush;
}

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
        for (size_t run = 0; run < num_runs; run++) {
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
            std::cout << "." << std::flush;
        };
        return {};
    }
    ParseHeader(Pusher &batch_handle) : m_pusher(batch_handle) {}
};
