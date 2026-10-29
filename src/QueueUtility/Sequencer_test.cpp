#include <catch2/catch_test_macros.hpp>

#include "QueueUtility/Sequencer.hpp"
#include "Batch.hpp"
#include "../Batch.hpp"
#include "../Batch_Pool.hpp"
#include "Utility/SPSCQueue.h"

struct U64Batch;
using SPSC = rigtorp::SPSCQueue<U64BatchPtr>;

TEST_CASE("Sequencer", "[Sequencer]") {
    Batch_Pool<U64Batch> batch_pool(100);
    Sequencer<U64BatchPtr> sequence;
    for (size_t i = 0; i < 100; i++) {
        auto batch = batch_pool.acquire_batch_blocking();
        sequence(batch);
        REQUIRE(batch->sequence_number==i);
    }
}