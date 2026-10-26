#include "Merging_Buffer.hpp"
#include <sys/types.h>
#include "Batch.hpp"
#include "Batch_Pool.hpp"
#include "Compare.hpp"
#include "Constants.hpp"
#include "MPMCQueue.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstdint>
#include <print>

using Queue = rigtorp::SPSCQueue<U64BatchPtr>;

namespace {
TEST_CASE("MergingBuffer consume sequenced batches") {
    std::array<Queue, 4> m_in_queue = {Queue(64), Queue(64), Queue(64),
                                       Queue(64)};
    std::array<Queue*, 4> m_in_queue_ptr = {
        m_in_queue.data(), m_in_queue.data() + 1, m_in_queue.data() + 2,
        m_in_queue.data() + 3};
    Queue output_queue(64);
    Batch_Pool<U64Batch> pool(128);
    Merger<Queue, Compare_Triggers, 4> merger(m_in_queue_ptr);

    const auto num_queues = GENERATE(1, 2, 3, 4);
    constexpr auto NUM_BATCHES = 2;
    {
        int sequence_nr = 0;
        uint64_t inhalt_nr = 0;
        for (int i = 0; i < NUM_BATCHES; ++i) {
            const auto queue = rand() % num_queues;
            auto ba = pool.acquire_batch_blocking();
            ba->sequence_number = sequence_nr++;
            for (uint64_t i = 0; i < BATCH_SIZE; ++i) {
                ba->data[i] = inhalt_nr++;
            }
            while (not m_in_queue[queue].try_push(std::move(ba))) {
            }
        }
    }
    {
        uint64_t inhalt_nr = 0;
        for (uint64_t i = 0; i < (NUM_BATCHES - 1); ++i) {
            CAPTURE(i);
            REQUIRE(merger.front());
            auto batch = merger.front();

            for (auto d : *batch) {
                REQUIRE(d == inhalt_nr++);
            }
            merger.pop();
        }
        REQUIRE(merger.front() == nullptr);
    }
}
}  // namespace
