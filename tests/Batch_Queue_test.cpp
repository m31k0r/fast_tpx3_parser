
#include "Batch.hpp"
#include "Batch_Pool.hpp"
#include "Compare.hpp"
#include "Constants.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstdint>
#include <print>
#include "Batch_Queue.hpp"
#include "Utility/SPSCQueue.h"

using Queue = rigtorp::SPSCQueue<U64BatchPtr>;

namespace {
TEST_CASE("BatchQueue to single Queue") {
    Queue input_queue(64);
    Batch_Pool<U64Batch> pool(128);
    BatchQueue<Queue> batch(input_queue);

    constexpr auto NUM_BATCHES = 64;
    {
        uint64_t inhalt_nr = 0;
        for (size_t i = 0; i < NUM_BATCHES; ++i) {
            auto ba = pool.acquire_batch_blocking();
            for (size_t i = 0; i < BATCH_SIZE; ++i) {
                ba->data[i] = inhalt_nr++;
            }
            while (not input_queue.try_push(std::move(ba))) {
            }
        }
    }
    {
        uint64_t sequence_nr = 0;
        for (size_t i = 0; i < (NUM_BATCHES)*BATCH_SIZE; ++i) {
            REQUIRE(batch.front());
            REQUIRE(*batch.front() == sequence_nr++);
            batch.pop();
        }
        REQUIRE(batch.front() == nullptr);
    }
}
}  // namespace
