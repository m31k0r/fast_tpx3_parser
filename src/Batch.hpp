#pragma once
#include <array>
#include <cstdint>
#include <iostream>
#include <libassert/assert.hpp>
#include <new>
#include "Batch_Pool.hpp"
#include "Constants.hpp"

template <typename T>
struct alignas(std::hardware_destructive_interference_size) Batch {
    std::array<T, BATCH_SIZE> data{};
    size_t insert_idx{0};
    size_t sequence_number{0};

    auto cbegin() const { return data.cbegin(); }
    auto cend() const { return data.cend(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }
    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
};

template <typename T>
using BatchPtr = typename Batch_Pool<Batch<T>>::BatchPtr;
using U64BatchPtr = BatchPtr<uint64_t>;

template <typename T, typename Queue>
void exchange_batch_on_full(typename Batch_Pool<Batch<T>>::BatchPtr &batch,
                            Batch_Pool<Batch<T>> &batch_pool, Queue &queue) {
    DEBUG_ASSERT(batch, "Null batch in exchange_batch_on_full");
    if (batch->insert_idx >= BATCH_SIZE) {
        const auto new_seq_nr = batch->sequence_number + 1;
        while (not queue.try_push(std::move(batch))) {
        }
        batch = batch_pool.acquire_batch_blocking();
        batch->sequence_number = new_seq_nr;
    }
}
