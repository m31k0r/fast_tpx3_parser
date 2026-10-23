#include "Batch_Pool.hpp"
#include <cstdint>
#include "Batch.hpp"
#include "Constants.hpp"
#include "SPSCQueue.h"
#include "catch2/catch_all.hpp"

TEST_CASE("Pop and release with a batch pool") {
    constexpr auto bp_size = 32;
    Batch_Pool<Batch<uint64_t>> batch_pool(bp_size);
    std::vector<U64BatchPtr> batches;
    for (int i = 0; i < bp_size; ++i) {
        U64BatchPtr batch = batch_pool.acquire_batch_blocking();
        batches.push_back(std::move(batch));
    }
    REQUIRE(batch_pool.available_batches() == 0);
    while (not batches.empty()) {
        batches.pop_back();
    }
    REQUIRE(batch_pool.available_batches() == bp_size);
}

TEST_CASE("Deletion order") {
    constexpr auto bp_size = 32;
    Batch_Pool<Batch<uint64_t>> batch_pool(bp_size);
    auto batch = batch_pool.acquire_batch_blocking();
    batch = batch_pool.acquire_batch_blocking();
    REQUIRE(batch_pool.available_batches() == 31);
}

TEST_CASE("Exchange batch on full seq. nr.") {
    constexpr auto bp_size = 64;
    rigtorp::SPSCQueue<U64BatchPtr> queue(64);
    Batch_Pool<Batch<uint64_t>> batch_pool(bp_size);
    auto batch = batch_pool.acquire_batch_blocking();
    uint64_t seq_nr = 0;
    REQUIRE(batch->sequence_number == seq_nr++);
    batch->insert_idx = BATCH_SIZE;
    for (int i = 0; i < 100; ++i) {
        exchange_batch_on_full(batch, batch_pool, queue);
        REQUIRE(batch->sequence_number == seq_nr++);
        batch->insert_idx = BATCH_SIZE;
        if (queue.front()) {
            queue.pop();
        }
    }
}
