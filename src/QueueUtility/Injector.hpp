#pragma once
#include "QueueUtility/Sequencer.hpp"
#include "Constants.hpp"
#include "QueueConcepts.hpp"
template<typename OutQueue, typename BatchPtr, typename BatchPool, typename T>

class Injector {
    OutQueue &queue;
    BatchPool &pool;
    BatchPtr current_batch;
    Sequencer<BatchPtr> seq;

    BatchPtr next_batch() {
        auto batch = pool.acquire_batch_blocking();
        seq(batch);
        return batch;
    };

    bool inject() {
        if constexpr (InQueueable<OutQueue, BatchPtr>) {
            if (not queue.try_push(std::move(current_batch))) {
                return false;
            }
        } else {
            if (not queue[current_batch->sequence_number % queue.size()]
                .try_push(std::move(current_batch))) {
                return false;
                }
        }
        return true;
    }

public:
    Injector(OutQueue &queue, BatchPool &pool) : queue(queue), pool(pool) {
    }

    bool try_push(T val) {
        if (not current_batch) {
            current_batch = next_batch();
        }

        if (current_batch->size >= BATCH_SIZE) {
            auto old_seq [[maybe_unused]] = current_batch->sequence_number;
            if (not inject()) return false;
            current_batch = next_batch();
            DEBUG_ASSERT(current_batch->size == 0,
                         "New Batch should have zero size");
            DEBUG_ASSERT(old_seq + 1 == current_batch->sequence_number,
                         "Sequence should be kept");
        }

        current_batch->data[current_batch->size] = std::forward<T>(val);
        current_batch->size++;

        return true;
    }
};