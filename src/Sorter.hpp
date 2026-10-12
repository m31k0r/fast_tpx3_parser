#pragma once
#include <libassert/assert.hpp>
#include <thread>

#include "Batch.hpp"
#include "QueueConcepts.hpp"
#include "Sorting_Buffer.hpp"

template <typename InQueue, typename OutQueue, typename Compare,
          size_t WINDOW_SIZE = 64>
    requires MCQueueable<InQueue, U64BatchPtr> &&
             SCQueueable<OutQueue, U64BatchPtr>
class Sort {
    InQueue &m_hit_queue;
    OutQueue &m_sorted_hit_queue;
    SortingBuffer<uint64_t, WINDOW_SIZE, Compare> m_buffer;

    Batch_Pool<Batch<uint64_t>> &m_batch_pool;
    U64BatchPtr m_current_batch;

    std::jthread m_thread;

   public:
    Sort(InQueue &input_queue, OutQueue &output_queue,
         Batch_Pool<Batch<uint64_t>> &batch_pool)
        : m_hit_queue(input_queue),
          m_sorted_hit_queue(output_queue),
          m_batch_pool(batch_pool) {
        m_current_batch = m_batch_pool.acquire_batch_blocking();
        DEBUG_ASSERT(m_current_batch);
    }

    void work() {
        U64BatchPtr batch;
        if (m_hit_queue.try_pop(batch)) {
            DEBUG_ASSERT(batch);
            for (size_t i = 0; i < batch->insert_idx; ++i) {
                auto data = batch->data[i];
                auto sorted_data = m_buffer.pushpop(data);
                if (sorted_data.has_value()) {
                    DEBUG_ASSERT(m_current_batch,
                                 "m_current_batch should not be nullptr");
                    exchange_batch_on_full(m_current_batch, m_batch_pool,
                                           m_sorted_hit_queue);
                    m_current_batch->data[m_current_batch->insert_idx++] =
                        sorted_data.value();
                }
            }
        }
    }
    void start_worker() {
        m_thread = std::jthread([this](std::stop_token stop) {
            if (m_current_batch == nullptr) {
                UNREACHABLE();
                m_current_batch = m_batch_pool.acquire_batch_blocking();
            }
            while (not stop.stop_requested()) {
                work();
            }
        });
    }
};
