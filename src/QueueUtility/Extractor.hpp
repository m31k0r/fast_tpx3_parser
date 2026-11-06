#pragma once
#include <assert.hpp>
#include <deque>
#include <memory>
#include <type_traits>

#include "../Batch/Batch.hpp"
#include "QueueConcepts.hpp"

#include "Iterator.hpp"

template <typename InQueue, typename BatchPtr>
    requires SCQueueable<InQueue, BatchPtr>

class Extractor {
    using Batch = typename BatchPtr::element_type;
    using OwnedBatchPtr = std::unique_ptr<Batch>;

    using IteratorType = std::conditional_t<
        std::is_same_v<BatchPtr, HitBatchPtr>, HitIterator,
        std::conditional_t<std::is_same_v<BatchPtr, TriggerBatchPtr>,
                           TriggerIterator, U64Iterator>>;

    InQueue &queue;
    size_t read_idx = 0;

    OwnedBatchPtr current_batch;

    std::deque<OwnedBatchPtr> overflow;

    OwnedBatchPtr next_batch() {
        if (not overflow.empty()) {
            auto tmp = std::move(overflow.front());
            overflow.pop_front();
            read_idx = 0;
            return tmp;
        }
        if (auto *queued = queue.front()) {
            auto tmp = std::make_unique<Batch>(**queued);
            queue.pop();
            read_idx = 0;
            return tmp;
        }
        return nullptr;
    }

   public:
    bool flush = false;
    Extractor(InQueue &queue) : queue(queue) {}

    IteratorType front() {
        if (not current_batch) {
            current_batch = next_batch();
        }
        if (not current_batch) {
            return IteratorType(nullptr, 0);
        }
        if (read_idx >= current_batch->size) {
            if (current_batch->size < BATCH_SIZE) flush = true;
            current_batch = next_batch();
            if (not current_batch) {
                return IteratorType(nullptr, 0);
            }
        }
        return IteratorType(current_batch.get(), read_idx);
    };

    void pop() {
        DEBUG_ASSERT(current_batch);
        DEBUG_ASSERT(read_idx < current_batch->size);
        read_idx++;
    };
    // Copy queued batches into an unbounded local buffer, returning the
    // originals to the pool so the producer can continue.
    void drain() {
        while (auto *queued = queue.front()) {
            overflow.push_back(std::make_unique<Batch>(**queued));
            queue.pop();
        }
    }
    size_t size() { return queue.size() + overflow.size(); }
};
