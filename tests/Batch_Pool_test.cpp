#include "Batch_Pool.hpp"
#include <cstdint>
#include "Batch.hpp"
#include "flux.hpp"
#include "gtest/gtest.h"

namespace {
TEST(BatchPoolTest, ConstructingABatchPool) {
    Batch_Pool<Batch<uint64_t>, 64> batch;
}
TEST(BatchPoolTest, PopAndRelease) {
    constexpr auto bp_size = 32;
    Batch_Pool<Batch<uint64_t>, bp_size> batch_pool;
    std::vector<Batch<uint64_t>*> batches;
    for (auto _ : flux::iota(0, bp_size)) {
        auto batch = batch_pool.acquire_batch_blocking();
        batches.push_back(batch);
    }
    EXPECT_EQ(batch_pool.available_batches(), 0);
    while (not batches.empty()) {
        batch_pool.release_batch_blocking(batches.back());
        batches.pop_back();
    }
    EXPECT_EQ(batch_pool.available_batches(), bp_size);
}
}  // namespace
