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
        for (auto u64 : *batch) {
            auto trigger = tpx3_helper::Trigger(u64);
            out_batch->push_back(trigger.get_toa(), 0, trigger.get_type());
        }
        return out_batch;
    }

    ParseTrigger(Batch_Pool<TriggerBatch> &t_batch_pool)
        : batch_pool(t_batch_pool) {}
};

class ParseHeader {
    BatchHandle<2> &m_batch_handle;

   public:
    void parse_header(tpx3_helper::Header t_header) {
        for (const auto *data_ptr = t_header.start; data_ptr < t_header.end;
             data_ptr = std::next(data_ptr)) {
            const uint64_t data_entry = *data_ptr;
            auto nibble = to_nibble(data_entry);

            switch (nibble) {
                case HIT_NIBBLE: {
                    m_batch_handle.insert_hit(data_entry, t_header.chip_nr);
                    break;
                }
                case TRIGGER_NIBBLE: {
                    m_batch_handle.insert_trigger(data_entry);
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
            }
            std::cout << ".";
        };
        return {};
    }
    ParseHeader(BatchHandle<2> &batch_handle) : m_batch_handle(batch_handle) {}
};
