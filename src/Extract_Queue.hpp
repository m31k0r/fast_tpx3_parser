#pragma once
#include <assert.hpp>
#include "Batch.hpp"
#include "Batch_Pool.hpp"
#include "QueueConcepts.hpp"
template <typename InQueue, typename BatchPtr>
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
        explicit operator bool() const noexcept { return batch != nullptr; }
        Iterator(BatchPtr *batch, size_t idx) : batch(batch), idx(idx) {}
        uint64_t toa() const { return get_toa(); }
        uint64_t tot() const { return get_tot(); }
        uint16_t x() const { return get_x(); }
        uint16_t y() const { return get_y(); }
    };

    InQueue &queue;

    BatchPtr current_batch;

    BatchPtr next_batch() {
        if (queue.front()) {
            auto tmp = std::move(*queue.front());
            queue.pop();
            tmp->size = 0;
            return tmp;
        }
        return nullptr;
    }

   public:
    Extractor(InQueue &queue) : queue(queue) {}

    Iterator front() {
        if (not current_batch) {
            current_batch = next_batch();
        }
        if (not current_batch) {
            return Iterator(nullptr, 0);
        }
        if (current_batch->size >= BATCH_SIZE) {
            current_batch = next_batch();
            if (not current_batch) {
                return Iterator(nullptr, 0);
            }
        }
        return Iterator(&current_batch, current_batch->size);
    };
    void pop() {
        DEBUG_ASSERT(current_batch);
        DEBUG_ASSERT(current_batch->size < BATCH_SIZE);
        current_batch->size++;
    };
};

template <typename Batch>
struct Sequencer {
    uint64_t sequence_number = 0;
    void operator()(Batch &batch) {
        batch->sequence_number = sequence_number;
        sequence_number++;
    }
};

template <typename OutQueue, typename BatchPtr, typename BatchPool, typename T>

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
    Injector(OutQueue &queue, BatchPool &pool) : queue(queue), pool(pool) {}
    bool try_push(T val) {
        if (not current_batch) {
            current_batch = next_batch();
        }

        if (current_batch->size >= BATCH_SIZE) {
            auto old_seq [[maybe_unused]] = current_batch->sequence_number;
            inject();
            current_batch = next_batch();
            DEBUG_ASSERT(current_batch->size == 0,
                         "New Batch should have zero size");
            DEBUG_ASSERT(old_seq + 1 == current_batch->sequence_number,
                         "Seuqence should be kept");
        }

        current_batch->data[current_batch->size] = std::forward<T>(val);
        current_batch->size++;

        return true;
    }
};
