#pragma once
#include <chrono>
#include <cstdint>
#include "Batch.hpp"
#include "Constants.hpp"
#include "Memory_Map.hpp"
#include "tpx3_helper.hpp"

class BatchHandle {
   public:
    Batch<uint64_t> *hit_batches[4];
    Batch<uint64_t> *trigger_batch;
    Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> batch_pool;

    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> m_hit_queue[4];
    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> m_trigger_queue;

    void insert_release(
        Batch<uint64_t> *&batch,
        es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &queue,
        uint64_t data) {
        exchange_batch_on_full(batch, batch_pool, queue);
        batch->data[batch->insert_idx++] = data;
    }

    void insert_hit(uint8_t chip_nr, uint64_t data) {
        insert_release(hit_batches[chip_nr], m_hit_queue[chip_nr], data);
    }
    void insert_trigger(uint64_t data) {
        insert_release(trigger_batch, m_trigger_queue, data);
    }

    BatchHandle() {
        for (auto &batch : hit_batches) {
            batch = batch_pool.acquire_batch_blocking();
        }
        trigger_batch = batch_pool.acquire_batch_blocking();
    }
};

class Parser {
    BatchHandle m_batch_handle;

   public:
    auto get_batch_handle() -> BatchHandle & { return m_batch_handle; }

    void parse_header(tpx3_helper::Header t_header) {
        for (const auto *data_ptr = t_header.start; data_ptr < t_header.end;
             data_ptr = std::next(data_ptr)) {
            const uint64_t data_entry = *data_ptr;
            auto nibble = to_nibble(data_entry);

            switch (nibble) {
                case HIT_NIBBLE: {
                    m_batch_handle.insert_hit(t_header.chip_nr, data_entry);
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

    int parse(MemoryMap &t_mmap) {
        std::chrono::high_resolution_clock::time_point start;
        start = std::chrono::high_resolution_clock::now();
        for (size_t run = 0; run < num_runs; run++) {
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
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "Throughput: "
                  << std::distance(t_mmap.begin(), t_mmap.end()) / 1024.0 /
                         1024.0 / 1024.0 * 8 /
                         std::chrono::duration<double>(end - start).count() *
                         num_runs
                  << " GB/s" << std::endl;
        return 0;
    };
};
