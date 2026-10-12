#include "Merging_Buffer.hpp"
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

using queue = rigtorp::SPSCQueue<U64BatchPtr>;

namespace {
TEST_CASE("MergingBuffer consume sequenced batches") {
    std::array<queue, 4> m_in_queue = {queue(64), queue(64), queue(64),
                                       queue(64)};
    std::array<queue*, 4> m_in_queue_ptr = {
        m_in_queue.data(), m_in_queue.data() + 1, m_in_queue.data() + 2,
        m_in_queue.data() + 3};
    queue output_queue(64);
    Batch_Pool<Batch<uint64_t>> pool(128);
    Merger<queue, queue, Compare_Triggers, 4> merger(m_in_queue_ptr,
                                                     output_queue);

    const auto num_queues = GENERATE(1, 2, 3, 4);
    constexpr auto NUM_BATCHES = 64;
    int sequence_nr = 0;
    for (int i = 0; i < NUM_BATCHES; ++i) {
        const auto queue = rand() % num_queues;
        auto ba = pool.acquire_batch_blocking();
        ba->sequence_number = sequence_nr++;
        while (not m_in_queue[queue].try_push(std::move(ba))) {
        }
    }
    sequence_nr = 0;
    for (int i = 0; i < NUM_BATCHES; ++i) {
        U64BatchPtr ba;
        while (not(ba = merger.next_batch()));
        if (ba) {
            CAPTURE(sequence_nr);
            REQUIRE(sequence_nr++ == ba->sequence_number);
            // std::println("Poped seq. nr. {}", ba->sequence_number);
        }
    }
}
}  // namespace
