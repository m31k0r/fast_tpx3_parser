#pragma once
#include <assert.hpp>
#include <deque>
#include <memory>
#include <type_traits>

#include "../Batch/Batch.hpp"
#include "Constants.hpp"
#include "QueueConcepts.hpp"

template <typename InQueue, typename BatchPtr>
    requires SCQueueable<InQueue, BatchPtr>

class Extractor {
    using Batch = typename BatchPtr::element_type;
    using OwnedBatchPtr = std::unique_ptr<Batch>;

    class HitIterator {
        OwnedBatchPtr *batch;
        size_t idx;

        [[nodiscard]] uint64_t get_toa() const {
            DEBUG_ASSERT(batch);
            DEBUG_ASSERT(idx < BATCH_SIZE);
            return (*batch)->toa[idx];
        }

        [[nodiscard]] uint64_t get_tot() const {
            DEBUG_ASSERT(batch);
            DEBUG_ASSERT(idx < BATCH_SIZE);
            return (*batch)->tot[idx];
        }

        [[nodiscard]] uint16_t get_x() const {
            DEBUG_ASSERT(batch);
            DEBUG_ASSERT(idx < BATCH_SIZE);
            return (*batch)->x[idx];
        }

        [[nodiscard]] uint16_t get_y() const {
            DEBUG_ASSERT(batch);
            DEBUG_ASSERT(idx < BATCH_SIZE);
            return (*batch)->y[idx];
        }

       public:
        explicit operator bool() const noexcept { return batch != nullptr; }

        HitIterator(OwnedBatchPtr *batch, size_t idx) : batch(batch), idx(idx) {}

        uint64_t toa() const { return get_toa(); }
        uint64_t tot() const { return get_tot(); }
        uint16_t x() const { return get_x(); }
        uint16_t y() const { return get_y(); }
    };
    class TriggerIterator {
        OwnedBatchPtr *batch;
        size_t idx;

        [[nodiscard]] uint64_t get_toa() const {
            DEBUG_ASSERT(batch);
            DEBUG_ASSERT(idx < BATCH_SIZE);
            return (*batch)->toa[idx];
        }

        [[nodiscard]] uint8_t get_type() const {
            DEBUG_ASSERT(batch);
            DEBUG_ASSERT(idx < BATCH_SIZE);
            return (*batch)->type[idx];
        }

       public:
        explicit operator bool() const noexcept { return batch != nullptr; }

        TriggerIterator(OwnedBatchPtr *batch, size_t idx)
            : batch(batch), idx(idx) {}

        uint64_t toa() const { return get_toa(); }
        uint8_t type() const { return get_type(); }
    };

    struct U64Iterator {
        uint64_t *ptr = nullptr;

        U64Iterator(const OwnedBatchPtr *batch, size_t idx) {
            if (batch) {
                ptr = &(*batch)->data[idx];
            }
        }
        uint64_t operator*() const {
            DEBUG_ASSERT(ptr);
            return *ptr;
        }
        explicit operator bool() const noexcept { return ptr != nullptr; }
    };

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
