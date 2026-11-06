#pragma once
#include <assert.hpp>
#include <deque>

#include "../Batch/Batch.hpp"
#include "QueueConcepts.hpp"

#include "Iterator.hpp"

template <typename InQueue, typename BatchPtr>
    requires SCQueueable<InQueue, BatchPtr>

class Extractor {
    using IteratorType = std::conditional_t<
        std::is_same_v<BatchPtr, HitBatchPtr>, HitIterator,
        std::conditional_t<std::is_same_v<BatchPtr, TriggerBatchPtr>,
                           TriggerIterator, U64Iterator>>;

    InQueue &queue;
    size_t read_idx = 0;
    BatchPtr current_batch;

    std::deque<BatchPtr> overflow;

    BatchPtr next_batch() {
        if (not overflow.empty()) {
            auto tmp = std::move(overflow.front());
            overflow.pop_front();
            read_idx = 0;
            return tmp;
        }
        return nullptr;
    }

   public:
    Extractor(InQueue &queue) : queue(queue) {}

    IteratorType front() {
        if (not current_batch) {
            current_batch = next_batch();
        }
        if (not current_batch) {
            return IteratorType(nullptr, 0);
        }
        if (read_idx >= current_batch->size) {
            current_batch = next_batch();
            if (not current_batch) {
                return IteratorType(nullptr, 0);
            }
        }
        return IteratorType(&current_batch, read_idx);
    };

    void pop() {
        DEBUG_ASSERT(current_batch);
        DEBUG_ASSERT(read_idx < current_batch->size);
        read_idx++;
    };
    // Moves all queued batches into an unbounded local buffer so the
    // producer never blocks on a full queue.
    void drain() {
        if (queue.front()) {
            overflow.push_back(std::move(*queue.front()));
            queue.pop();
        }
    }
    size_t size() { return queue.size() + overflow.size(); }
};
