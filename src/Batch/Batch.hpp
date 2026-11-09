#pragma once
#include <array>
#include <cstdint>
#include <iostream>
#include <libassert/assert.hpp>
#include <new>
#include "../Constants.hpp"
#include "../Parsing/tpx3_helper.hpp"
#include "Batch_Pool.hpp"

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
    void push_back(uint64_t t_data) {
        DEBUG_ASSERT(size <= BATCH_SIZE);
        data[size] = t_data;
        size++;
    }
};
struct alignas(std::hardware_destructive_interference_size) TriggerBatch {
    std::array<uint64_t, BATCH_SIZE> toa{};
    std::array<uint8_t, BATCH_SIZE> type{};
    size_t size{0};
    size_t sequence_number{0};
    void push_back(uint64_t t_toa, uint8_t t_type) {
        DEBUG_ASSERT(size < BATCH_SIZE);
        toa[size] = t_toa;
        type[size] = t_type;
        size++;
    }
};
struct Hit {
    uint64_t toa;
    uint64_t tot;
    uint16_t x;
    uint16_t y;
    uint16_t delay = 0;
    uint16_t trigger_diff = 0;
};
struct alignas(std::hardware_destructive_interference_size) HitBatch {
    std::array<uint64_t, BATCH_SIZE> toa{};
    std::array<uint64_t, BATCH_SIZE> tot{};
    std::array<uint16_t, BATCH_SIZE> x{};
    std::array<uint16_t, BATCH_SIZE> y{};

    std::array<uint16_t, BATCH_SIZE> delay{};
    std::array<uint16_t, BATCH_SIZE> trigger_diff{};

    size_t size{0};
    size_t sequence_number{0};
    void push_back(Hit hit) {
        DEBUG_ASSERT(size <= BATCH_SIZE);
        toa[size] = hit.toa;
        tot[size] = hit.tot;
        x[size] = hit.x;
        y[size] = hit.y;
        delay[size] = hit.delay;
        trigger_diff[size] = hit.trigger_diff;
        size++;
    }
};

using U64BatchPtr = typename Batch_Pool<U64Batch>::BatchPtr;
using TriggerBatchPtr = typename Batch_Pool<TriggerBatch>::BatchPtr;
using HitBatchPtr = typename Batch_Pool<HitBatch>::BatchPtr;
