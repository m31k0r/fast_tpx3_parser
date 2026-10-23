#pragma once
#include <assert.hpp>
#include "Batch.hpp"
#include "Batch_Pool.hpp"
#include "QueueConcepts.hpp"
template <typename InQueue, typename T>
    requires SCQueueable<InQueue, BatchPtr<T>>

class Extractor {
    InQueue &queue;

    BatchPtr<T> current_batch;

    BatchPtr<T> next_batch() {
        if (queue.front()) {
            auto tmp = std::move(*queue.front());
            queue.pop();
            tmp->insert_idx = 0;
            return tmp;
        }
        return nullptr;
    }

   public:
    Extractor(InQueue &queue) : queue(queue) {}

    uint64_t *front() {
        if (not current_batch) {
            current_batch = next_batch();
        }
        if (not current_batch) {
            return nullptr;
        }
        if (current_batch->insert_idx >= BATCH_SIZE) {
            current_batch = next_batch();
            if (not current_batch) {
                return nullptr;
            }
        }
        return &current_batch->data[current_batch->insert_idx];
    };
    void pop() {
        DEBUG_ASSERT(current_batch);
        DEBUG_ASSERT(current_batch->insert_idx < BATCH_SIZE);
        current_batch->insert_idx++;
    };
};

template <typename OutQueue, typename T>
    requires InQueueable<OutQueue, BatchPtr<T>>

class Injector {
    OutQueue &queue;
    Batch_Pool<Batch<T>> &pool;
    BatchPtr<T> current_batch;
    BatchPtr<T> next_batch() {
        auto batch = pool.acquire_batch_blocking();
        batch->insert_idx = 0;
        return batch;
    };

   public:
    Injector(OutQueue &queue, Batch_Pool<Batch<T>> &pool)
        : queue(queue), pool(pool) {}
    bool try_push(T &&val) {
        if (not current_batch) {
            current_batch = next_batch();
        }

        if (current_batch->insert_idx >= BATCH_SIZE) {
            if (!queue.try_push(std::move(current_batch))) {
                return false;
            }

            current_batch = next_batch();
        }

        current_batch->data[current_batch->insert_idx++] = std::forward<T>(val);

        return true;
    }
};
