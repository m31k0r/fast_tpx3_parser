#include "Merging_Buffer.hpp"
#include <cstdint>
#include "Batch_Pool.hpp"
#include "Compare.hpp"
#include "Constants.hpp"
#include "mpmc_queue.h"

#include "gtest/gtest.h"

using queue = es::lockfree::mpmc_queue<Batch<uint64_t>*, QUEUE_SIZE>;

namespace {
TEST(MergingBuffer, Constructing) {
    std::array<queue, 4> m_in_queue;
    std::array<queue*, 4> m_in_queue_ptr = {
        m_in_queue.data(), m_in_queue.data() + 1, m_in_queue.data() + 2,
        m_in_queue.data() + 3};
    queue output_queue;
    Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> pool;
    Merger<Compare_Triggers, 4> merger(m_in_queue_ptr, &output_queue);

    constexpr auto NUM_BATCHES = 100;
    int sequence_nr = 0;
    for (int i = 0; i < NUM_BATCHES; ++i) {
        const auto queue = rand() % 4;
        auto ba = pool.acquire_batch_blocking();
        ba->sequence_number = sequence_nr++;
        while (not m_in_queue[queue].push(ba)) {
        }
    }
    sequence_nr = 0;
    for (int i = 0; i < NUM_BATCHES; ++i) {
        auto ba = merger.next_batch();
        ASSERT_EQ(sequence_nr++, ba->sequence_number);
        pool.release_batch_blocking(ba);
    }
}
}  // namespace
