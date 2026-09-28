#pragma once
#include <thread>
#include "Batch.hpp"
#include "Sorting_Buffer.hpp"

template <typename Compare, size_t WINDOW_SIZE = 64>
class Sort {
    std::jthread m_thread;
    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &m_hit_queue;
    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &m_sorted_hit_queue;
    SortingBuffer<uint64_t, WINDOW_SIZE, Compare> m_buffer;

    Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> &m_batch_pool;
    Batch<uint64_t> *m_current_batch;

   public:
    bool idle;

    Sort(es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &input_queue,
         es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &output_queue,
         Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> &batch_pool)
        : m_hit_queue(input_queue),
          m_sorted_hit_queue(output_queue),
          m_batch_pool(batch_pool) {
        m_current_batch = m_batch_pool.acquire_batch_blocking();
    }
    void start_worker() {
        m_thread = std::jthread([this](std::stop_token stop) {
            Batch<uint64_t> *batch;
            while (not stop.stop_requested()) {
                if (m_hit_queue.pop(batch)) {
                    idle = false;
                    for (size_t i = 0; i < batch->insert_idx; ++i) {
                        auto data = batch->data[i];
                        auto sorted_data = m_buffer.pushpop(data);
                        if (sorted_data.has_value()) {
                            exchange_batch_on_full(m_current_batch,
                                                   m_batch_pool,
                                                   m_sorted_hit_queue);
                            m_current_batch
                                ->data[m_current_batch->insert_idx++] =
                                sorted_data.value();
                        }
                    }
                    m_batch_pool.release_batch_blocking(batch);
                } else {
                    idle = true;
                }
            }
        });
    }
    void stop() { m_thread.request_stop(); }
};
