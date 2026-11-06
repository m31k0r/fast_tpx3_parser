
#include <assert.hpp>
#include <cstddef>
#include "Batch/Batch.hpp"
#include "Constants.hpp"

class HitIterator {
    HitBatch *batch;
    size_t idx;

    [[nodiscard]] uint64_t get_toa() const {
        DEBUG_ASSERT(batch);
        DEBUG_ASSERT(idx < BATCH_SIZE);
        return batch->toa[idx];
    }

    [[nodiscard]] uint64_t get_tot() const {
        DEBUG_ASSERT(batch);
        DEBUG_ASSERT(idx < BATCH_SIZE);
        return batch->tot[idx];
    }

    [[nodiscard]] uint16_t get_x() const {
        DEBUG_ASSERT(batch);
        DEBUG_ASSERT(idx < BATCH_SIZE);
        return batch->x[idx];
    }

    [[nodiscard]] uint16_t get_y() const {
        DEBUG_ASSERT(batch);
        DEBUG_ASSERT(idx < BATCH_SIZE);
        return batch->y[idx];
    }

   public:
    explicit operator bool() const noexcept { return batch != nullptr; }

    HitIterator(HitBatch *batch, size_t idx) : batch(batch), idx(idx) {}

    uint64_t toa() const { return get_toa(); }
    uint64_t tot() const { return get_tot(); }
    uint16_t x() const { return get_x(); }
    uint16_t y() const { return get_y(); }
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
