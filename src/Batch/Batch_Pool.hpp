#pragma once
#include <sys/types.h>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <libassert/assert.hpp>
#include <memory>
#include <vector>
#include "../Utility/MPMCQueue.h"

template <typename Batch>
class Batch_Pool {
    struct Deleter {
        Batch_Pool *pool;

        void operator()(Batch *batch) const noexcept {
            DEBUG_ASSERT(pool);
            DEBUG_ASSERT(batch);
            pool->release_batch_blocking(batch);
        }
    };

    void release_batch_blocking(Batch *t_batch);

   public:
    using BatchPtr = std::unique_ptr<Batch, Deleter>;

    std::vector<std::unique_ptr<Batch>> m_batches;
    rigtorp::MPMCQueue<Batch *> m_free_batches;
    uint64_t sequence_number{0};

    explicit Batch_Pool(size_t capacity);
    ~Batch_Pool();
    std::unique_ptr<Batch, Deleter> acquire_batch_blocking();

    size_t available_batches();
    // Disable copying
    Batch_Pool(const Batch_Pool &) = delete;
    Batch_Pool &operator=(const Batch_Pool &) = delete;

    // Enable moving
    Batch_Pool(Batch_Pool &&) noexcept = default;
    Batch_Pool &operator=(Batch_Pool &&) noexcept = default;
};

template <typename Batch>
Batch_Pool<Batch>::~Batch_Pool() {
    DEBUG_ASSERT(m_free_batches.size() == m_batches.size());
}

template <typename Batch>
Batch_Pool<Batch>::Batch_Pool(size_t capacity)
    : m_batches(capacity), m_free_batches(capacity) {
    for (size_t i = 0; i < capacity; ++i) {
        auto batch = std::make_unique<Batch>();
        m_free_batches.push(batch.get());
        m_batches[i] = std::move(batch);
    }
}

template <typename Batch>
typename Batch_Pool<Batch>::BatchPtr
Batch_Pool<Batch>::acquire_batch_blocking() {
    Batch *batch = nullptr;
    while (not m_free_batches.try_pop(batch)) {
        //            std::this_thread::sleep_for(std::chrono::nanooseconds(10));
        std::cerr << "No free batches this should not happen\n";
        std::cerr << libassert::stacktrace();
        exit(-1);
    }
    DEBUG_ASSERT(batch, "should not pop a null batch");
    batch->size = 0;
    batch->sequence_number = sequence_number++;
    return BatchPtr(batch, Deleter{this});
}
template <typename Batch>
void Batch_Pool<Batch>::release_batch_blocking(Batch *t_batch) {
    DEBUG_ASSERT(t_batch, "Dont push a nullptr");
    while (not m_free_batches.try_push(t_batch)) {
        //            std::this_thread::sleep_for(std::chrono::nanoseconds(10));
    }
};
template <typename Batch>
size_t Batch_Pool<Batch>::available_batches() {
    return m_free_batches.size();
}
