#pragma once
#include <cassert>
#include <fstream>
#include <memory>
#include <vector>
#include "mpmc_queue.h"

template <typename Batch, size_t BATCH_POOL_SIZE>
class Batch_Pool {
   public:
    std::array<std::unique_ptr<Batch>, BATCH_POOL_SIZE> m_batches;
    es::lockfree::mpmc_queue<Batch *, BATCH_POOL_SIZE> m_free_batches;

    Batch_Pool();
    Batch *acquire_batch_blocking();
    void release_batch_blocking(Batch *t_batch);

    size_t available_batches();
};

template <typename Batch, size_t BATCH_POOL_SIZE>
Batch_Pool<Batch, BATCH_POOL_SIZE>::Batch_Pool() {
    for (size_t i = 0; i < BATCH_POOL_SIZE; ++i) {
        auto batch = std::make_unique<Batch>();
        m_free_batches.push(batch.get());
        m_batches[i] = std::move(batch);
    }
}

template <typename Batch, size_t BATCH_POOL_SIZE>
Batch *Batch_Pool<Batch, BATCH_POOL_SIZE>::acquire_batch_blocking() {
    Batch *batch = nullptr;
    while (not m_free_batches.pop(batch)) {
        //            std::this_thread::sleep_for(std::chrono::nanooseconds(10));
    }
    bool expected = false;

    if (!batch->in_use.compare_exchange_strong(expected, true,
                                               std::memory_order_acq_rel)) {
        assert(false && "Batch acquired while already in use");
    }

    batch->insert_idx = 0;
    return batch;
}
template <typename Batch, size_t BATCH_POOL_SIZE>
void Batch_Pool<Batch, BATCH_POOL_SIZE>::release_batch_blocking(
    Batch *t_batch) {
    bool expected = true;

    if (!t_batch->in_use.compare_exchange_strong(expected, false,
                                                 std::memory_order_acq_rel)) {
        assert(false && "Batch released twice");
    }
    while (not m_free_batches.push(t_batch)) {
        //            std::this_thread::sleep_for(std::chrono::nanoseconds(10));
    }
};
template <typename Batch, size_t BATCH_POOL_SIZE>
size_t Batch_Pool<Batch, BATCH_POOL_SIZE>::available_batches() {
    return m_free_batches.size();
}
