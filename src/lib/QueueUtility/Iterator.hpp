#pragma once
#include <libassert/assert.hpp>
#include <cstddef>
#include "Batch/Batch.hpp"
#include "Constants.hpp"

class HitIterator {
    HitBatch *batch;
    size_t idx;


    void assert_iterator() {
        DEBUG_ASSERT(batch);
        DEBUG_ASSERT(idx < BATCH_SIZE);
    }

   public:
    explicit operator bool() const noexcept { return batch != nullptr; }

    HitIterator(HitBatch *batch, size_t idx) : batch(batch), idx(idx) {}

    [[nodiscard]] auto toa() const { return batch->toa[idx]; }
    [[nodiscard]] auto tot() const { return batch->tot[idx]; }
    [[nodiscard]] auto x() const {return batch->x[idx]; }
    [[nodiscard]] auto y() const {return batch->y[idx]; }
    [[nodiscard]] auto delay() const {return batch->delay[idx]; }
    [[nodiscard]] auto trigger_diff() const {return batch->trigger_diff[idx]; }
    [[nodiscard]] auto cluster_size() const {return batch->cluster_size[idx]; }
    [[nodiscard]] auto cluster_tot() const {return batch->cluster_tot[idx]; }

    void toa(auto tot) { batch->toa[idx] = tot; }
    void tot(auto tot) { batch->tot[idx] = tot; }
    void x(auto x) { batch->x[idx] = x; }
    void y(auto y) { batch->y[idx] = y; }
    void delay(auto delay) { batch->delay[idx] = delay; }
    void trigger_diff(auto trigger_diff) { batch->trigger_diff[idx] = trigger_diff; }
    void cluster_size(auto cluster_size) {
        batch->cluster_size[idx] = cluster_size;
    }
    void cluster_tot(auto cluster_tot) {batch->cluster_tot[idx] = cluster_tot; }
};
class TriggerIterator {
    TriggerBatch *batch;
    size_t idx;

    [[nodiscard]] uint64_t get_toa() const {
        DEBUG_ASSERT(batch);
        DEBUG_ASSERT(idx < BATCH_SIZE);
        return batch->toa[idx];
    }

    [[nodiscard]] uint8_t get_type() const {
        DEBUG_ASSERT(batch);
        DEBUG_ASSERT(idx < BATCH_SIZE);
        return batch->type[idx];
    }

   public:
    explicit operator bool() const noexcept { return batch != nullptr; }

    TriggerIterator(TriggerBatch *batch, size_t idx) : batch(batch), idx(idx) {}

    uint64_t toa() const { return get_toa(); }
    uint8_t type() const { return get_type(); }
};

struct U64Iterator {
    const uint64_t *ptr = nullptr;

    U64Iterator(const U64Batch *batch, size_t idx) {
        if (batch) {
            ptr = &batch->data[idx];
        }
    }
    uint64_t operator*() const {
        DEBUG_ASSERT(ptr);
        return *ptr;
    }
    explicit operator bool() const noexcept { return ptr != nullptr; }
};
