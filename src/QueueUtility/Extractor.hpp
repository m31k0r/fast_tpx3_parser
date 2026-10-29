#pragma once
#include <assert.hpp>

#include "Batch.hpp"
#include "QueueConcepts.hpp"
#include "Constants.hpp"

template<typename InQueue, typename BatchPtr>
    requires SCQueueable<InQueue, BatchPtr>

class Extractor {
    class Iterator {
        BatchPtr *batch;
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
        explicit operator bool() const noexcept {
            DEBUG_ASSERT(batch);
            return batch != nullptr;
        }

        Iterator(BatchPtr *batch, size_t idx) : batch(batch), idx(idx) {
        }

        uint64_t toa() const { return get_toa(); }
        uint64_t tot() const { return get_tot(); }
        uint16_t x() const { return get_x(); }
        uint16_t y() const { return get_y(); }
    };

    struct U64Iterator {
        uint64_t *ptr = nullptr;

        U64Iterator(const BatchPtr *batch, size_t idx) {
            if (batch) { ptr = &(*batch)->data[idx]; }
        }
        uint64_t operator* () const {
            DEBUG_ASSERT(ptr);
            return *ptr;
        }
        explicit operator bool() const noexcept { return ptr != nullptr; }
    };

    using IteratorType = std::conditional_t<std::is_same_v<BatchPtr, HitBatchPtr>, Iterator, U64Iterator>;

    InQueue &queue;
    size_t read_idx = 0;
    BatchPtr current_batch;

    BatchPtr next_batch() {
        if (queue.front()) {
            auto tmp = std::move(*queue.front());
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
        if (read_idx >= current_batch->size ) {
            current_batch = next_batch();
            if (not current_batch) {
                return IteratorType(nullptr, 0);
            }
        }
        return IteratorType(&current_batch, read_idx);
    };

    void pop() {
        DEBUG_ASSERT(current_batch);
        DEBUG_ASSERT(read_idx < current_batch->size );
        read_idx++;
    };
};