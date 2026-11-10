#include <catch2/catch_test_macros.hpp>

#include "Operations/Sink.hpp"
#include "Utility/SPSCQueue.h"

TEST_CASE("Sink consumes a batch and recognizes EOF messages") {
    using Message = QueueMessage<HitBatchPtr>;
    rigtorp::SPSCQueue<Message> queue(2);
    Batch_Pool<HitBatch> pool(1);
    Sink<decltype(queue), HitBatchPtr> sink;
    auto batch = pool.acquire_batch_blocking();
    batch->push_back({1, 2, 3, 4});
    queue.push(std::move(batch));
    REQUIRE_FALSE(sink(queue));
    REQUIRE(queue.size() == 0);
    queue.push(EndOfFile{});
    REQUIRE(sink(queue));
    REQUIRE(queue.size() == 0);
}
