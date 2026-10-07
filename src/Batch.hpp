#pragma once
#include <array>
#include <atomic>
#include "Batch_Pool.hpp"
#include "Constants.hpp"

template <typename T>
struct Batch {
    std::array<T, BATCH_SIZE> data{};
    size_t insert_idx{0};
    size_t sequence_number{0};
    std::atomic<bool> in_use{false};
};

template <typename Batch>
void exchange_batch_on_full(
    Batch *&batch, Batch_Pool<Batch, BATCH_POOL_SIZE> &batch_pool,
    es::lockfree::mpmc_queue<Batch *, QUEUE_SIZE> &queue) {
    if (batch->insert_idx >= BATCH_SIZE) {
        const auto new_seq_nr = batch->sequence_number + 1;
        while (not queue.push(batch)) {
        }
        batch = batch_pool.acquire_batch_blocking();
        batch->sequence_number = new_seq_nr;
    }
}
