#include "Batch_Pool.hpp"
#include <cstdint>
#include "Batch.hpp"
#include "flux.hpp"
#include "gtest/gtest.h"

using BP = Batch_Pool<Batch<uint64_t>, 64>;
namespace {
TEST(BatchPoolTest, ConstructingABatchPool) { BP batch; }
TEST(BatchPoolTest, PopAndRelease) {
    constexpr auto bp_size = 32;
    BP batch_pool;
    std::vector<BP::BatchPtr> batches;
    for (auto _ : flux::iota(0, bp_size)) {
        auto batch = batch_pool.acquire_batch_blocking();
        batches.push_back(std::move(batch));
    }
    EXPECT_EQ(batch_pool.available_batches(), 0);
    while (not batches.empty()) {
        batches.pop_back();
    }
    EXPECT_EQ(batch_pool.available_batches(), bp_size);
}
}  // namespace
