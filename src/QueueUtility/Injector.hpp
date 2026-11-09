#pragma once
#include <iostream>
#include <libassert/assert.hpp>
#include <stop_token>
#include <type_traits>

#include "Constants.hpp"
#include "QueueConcepts.hpp"
#include "QueueUtility/QueueMessage.hpp"
#include "QueueUtility/Sequencer.hpp"
#include "Utility/AdaptivBackoff.hpp"
template <typename OutQueue, typename BatchPtr, typename BatchPool, typename T>

class Injector {
    OutQueue &queue;
    BatchPool &pool;
    BatchPtr current_batch;
    Sequencer<BatchPtr> seq;
    bool finished = false;

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
    Injector(OutQueue &queue, BatchPool &pool) : queue(queue), pool(pool) {}
    void flush() {
        if (not current_batch) return;
        adaptive_backoff backoff;
        while (not inject()) {
            backoff.pause();
        }
    };
    bool finish(std::stop_token stop = {}) {
        if (finished) return true;
        adaptive_backoff backoff;
        while (current_batch && not inject()) {
            if (stop.stop_requested()) return false;
            backoff.pause();
        }
        if constexpr (InQueueable<OutQueue, EndOfFile>) {
            while (not queue.try_push(EndOfFile{})) {
                if (stop.stop_requested()) return false;
                backoff.pause();
            }
        } else {
            for (auto &q : queue) {
                while (not q.try_push(EndOfFile{})) {
                    if (stop.stop_requested()) return false;
                    backoff.pause();
                }
            }
        }
        finished = true;
        return true;
    }
    bool try_push(T val);
};

template <typename OutQueue, typename BatchPtr, typename BatchPool, typename T>
inline bool Injector<OutQueue, BatchPtr, BatchPool, T>::try_push(T val) {
    ASSERT(not finished, "Cannot push after EOF");
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

    current_batch->push_back(val);
    return true;
}
