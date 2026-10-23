#pragma once

#include "Batch.hpp"
#include "QueueConcepts.hpp"
template <typename InQueue>
    requires InQueueable<InQueue, U64BatchPtr>
class BatchQueue {
    InQueue &m_queue;
    U64BatchPtr m_current_batch = nullptr;

    U64BatchPtr get_batch() {
        auto front = m_queue.front();
        if (not front) {
            return nullptr;
        }

        U64BatchPtr batch = std::move(*front);
        m_queue.pop();
        return batch;
    }

   public:
    BatchQueue(InQueue &queue) : m_queue(queue) {}
    uint64_t *front() {
        if (not m_current_batch) {
            m_current_batch = get_batch();
        }
        if (not m_current_batch) {
            return nullptr;
        }
        if (m_current_batch->insert_idx >= BATCH_SIZE) {
            m_current_batch = get_batch();
            if (not m_current_batch) {
                return nullptr;
            }
            m_current_batch->insert_idx = 0;
        }

        return &m_current_batch->data[m_current_batch->insert_idx];
    };
    void pop() {
        DEBUG_ASSERT(m_current_batch);
        DEBUG_ASSERT(m_current_batch->insert_idx < BATCH_SIZE);
        m_current_batch->insert_idx++;
    };
};
