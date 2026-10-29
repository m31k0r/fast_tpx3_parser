#pragma once
#include <array>
#include <cstdint>
#include <iostream>
#include <libassert/assert.hpp>
#include <new>
#include "Batch_Pool.hpp"
#include "Constants.hpp"
#include "tpx3_helper.hpp"

struct alignas(std::hardware_destructive_interference_size) U64Batch {
    std::array<uint64_t, BATCH_SIZE> data{};
    size_t size{0};
    size_t sequence_number{0};

    auto cbegin() const { return data.cbegin(); }
    auto cend() const { return data.cbegin() + size; }
    auto begin() const { return data.begin(); }
    auto end() const { return data.begin() + size; }
    auto begin() { return data.begin(); }
    auto end() { return data.begin() + size; }
};
struct alignas(std::hardware_destructive_interference_size) TriggerBatch {
    std::array<uint64_t, BATCH_SIZE> toa{};
    std::array<uint64_t, BATCH_SIZE> delay{};
    std::array<uint8_t, BATCH_SIZE> type{};
    size_t size{0};
    size_t sequence_number{0};
    void push_back(uint64_t t_toa, uint64_t t_delay, uint8_t t_type) {
        DEBUG_ASSERT(size < BATCH_SIZE);
        toa[size] = t_toa;
        delay[size] = t_delay;
        type[size] = t_type;
        size++;
    }
};
struct alignas(std::hardware_destructive_interference_size) HitBatch {
    std::array<uint64_t, BATCH_SIZE> toa{};
    std::array<uint64_t, BATCH_SIZE> tot{};
    std::array<uint16_t, BATCH_SIZE> x{};
    std::array<uint16_t, BATCH_SIZE> y{};
    size_t size{0};
    size_t sequence_number{0};
    void push_back(uint64_t t_toa, uint64_t t_tot, uint16_t t_x, uint16_t t_y) {
        DEBUG_ASSERT(size <= BATCH_SIZE);
        toa[size] = t_toa;
        tot[size] = t_tot;
        x[size] = t_x;
        y[size] = t_y;
        size++;
    }
};

using U64BatchPtr = typename Batch_Pool<U64Batch>::BatchPtr;
using TriggerBatchPtr = typename Batch_Pool<TriggerBatch>::BatchPtr;
using HitBatchPtr = typename Batch_Pool<HitBatch>::BatchPtr;

template <typename BatchPtr, typename BatchPool, typename Queue>
void exchange_batch_on_full(BatchPtr &batch, BatchPool &batch_pool,
                            Queue &queue) {
    DEBUG_ASSERT(batch, "Null batch in exchange_batch_on_full");
    if (batch->size >= BATCH_SIZE) {
        const auto new_seq_nr = batch->sequence_number + 1;
        while (not queue.try_push(std::move(batch))) {
        }
        batch = batch_pool.acquire_batch_blocking();
        batch->sequence_number = new_seq_nr;
    }
}
