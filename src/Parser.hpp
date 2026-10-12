#pragma once
#include <cstdint>
#include "Batch.hpp"
#include "Constants.hpp"
#include "MPMCQueue.h"
#include "Memory_Map.hpp"
#include "tpx3_helper.hpp"

class BatchHandle {
   public:
    Batch_Pool<Batch<uint64_t>> batch_pool{BATCH_POOL_SIZE};
    U64BatchPtr hit_batches[4];
    U64BatchPtr trigger_batch;

    using Queue = rigtorp::MPMCQueue<U64BatchPtr>;
    Queue m_hit_queue[4]{Queue{QUEUE_SIZE}, Queue{QUEUE_SIZE},
                         Queue{QUEUE_SIZE}, Queue{QUEUE_SIZE}};
    rigtorp::MPMCQueue<U64BatchPtr> m_trigger_queue{QUEUE_SIZE};

    void insert_release(U64BatchPtr &batch,
                        rigtorp::MPMCQueue<U64BatchPtr> &queue, uint64_t data) {
        exchange_batch_on_full(batch, batch_pool, queue);
        batch->data[batch->insert_idx++] = data;
        asm volatile("" ::"g"(batch->data[batch->insert_idx - 1]) : "memory");
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

    void parse_header(tpx3_helper::Header t_header);

    int parse(MemoryMap &t_mmap);
};
