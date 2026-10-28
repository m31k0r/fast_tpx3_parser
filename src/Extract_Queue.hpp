#pragma once
#include <assert.hpp>
#include "Batch.hpp"
#include "Batch_Pool.hpp"
#include "QueueConcepts.hpp"
// template <typename InQueue, typename T>
//     requires SCQueueable<InQueue, BatchPtr<T>>
//
// class Extractor {
//     InQueue &queue;
//
//     BatchPtr<T> current_batch;
//
//     BatchPtr<T> next_batch() {
//         if (queue.front()) {
//             auto tmp = std::move(*queue.front());
//             queue.pop();
//             tmp->insert_idx = 0;
//             return tmp;
//         }
//         return nullptr;
//     }
//
//    public:
//     Extractor(InQueue &queue) : queue(queue) {}
//
//     uint64_t *front() {
//         if (not current_batch) {
//             current_batch = next_batch();
//         }
//         if (not current_batch) {
//             return nullptr;
//         }
//         if (current_batch->insert_idx >= BATCH_SIZE) {
//             current_batch = next_batch();
//             if (not current_batch) {
//                 return nullptr;
//             }
//         }
//         return &current_batch->data[current_batch->insert_idx];
//     };
//     void pop() {
//         DEBUG_ASSERT(current_batch);
//         DEBUG_ASSERT(current_batch->insert_idx < BATCH_SIZE);
//         current_batch->insert_idx++;
//     };
// };
//
template <typename Batch>
struct Sequencer {
    uint64_t sequence_number = 0;
    void operator()(Batch &batch) {
        batch->sequence_number = sequence_number;
        sequence_number++;
    }
};

template <typename OutQueue, typename BatchPtr, typename BatchPool, typename T>
    requires InQueueable<OutQueue, BatchPtr>

class Injector {
    OutQueue &queue;
    BatchPool &pool;
    BatchPtr current_batch;
    Sequencer<U64BatchPtr> seq;

    BatchPtr next_batch() {
        auto batch = pool.acquire_batch_blocking();
        seq(batch);
        return batch;
    };

   public:
    Injector(OutQueue &queue, BatchPool &pool) : queue(queue), pool(pool) {}
    bool try_push(T val) {
        if (not current_batch) {
            current_batch = next_batch();
        }

        if (current_batch->size >= BATCH_SIZE) {
            auto old_seq [[maybe_unused]] = current_batch->sequence_number;
            if (not queue.try_push(std::move(current_batch))) {
                return false;
            }
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
