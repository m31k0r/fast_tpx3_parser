#include <catch2/catch_test_macros.hpp>

#include <array>
#include <variant>

#include "Batch/Batch.hpp"
#include "Batch/Batch_Pool.hpp"
#include "Constants.hpp"
#include "QueueUtility/Injector.hpp"
#include "Utility/SPSCQueue.h"
#include "Utility/helper.hpp"

struct U64Batch;
using SPSC = rigtorp::SPSCQueue<U64BatchPtr>;

TEST_CASE("Injector", "[Injector]") {
    Batch_Pool<U64Batch> batch_pool(100);
    SPSC spsc(100);
    uint64_t counter = 1;
    Injector<SPSC, U64BatchPtr, Batch_Pool<U64Batch>, uint64_t> injector(
        spsc, batch_pool);
    for (size_t i = 0; i < 100 * BATCH_SIZE; i++) {
        REQUIRE(injector.try_push(counter++));
    }
    counter = 1;
    for (size_t i = 0; i < 99; ++i) {
        U64BatchPtr *batch = spsc.front();
        REQUIRE(batch);
        REQUIRE(*batch);
        for (auto &b : **batch) {
            REQUIRE(b == counter++);
        }
        spsc.pop();
    }
}

TEST_CASE("Injector sends batches before EOF", "[Injector]") {
    using Message = QueueMessage<U64BatchPtr>;
    using MessageQueue = rigtorp::SPSCQueue<Message>;
    Batch_Pool<U64Batch> pool(3);
    MessageQueue queue(4);
    Injector<MessageQueue, U64BatchPtr, Batch_Pool<U64Batch>, uint64_t>
        injector(queue, pool);

    injector.flush();
    REQUIRE_FALSE(queue.front());
    for (size_t i = 0; i < BATCH_SIZE; ++i) {
        REQUIRE(injector.try_push(i));
    }
    injector.flush();
    injector.flush();
    REQUIRE(injector.try_push(42));
    injector.finish();
    injector.finish();

    REQUIRE(queue.front());
    auto &full = std::get<U64BatchPtr>(*queue.front());
    REQUIRE(full->size == BATCH_SIZE);
    REQUIRE(full->data[0] == 0);
    REQUIRE(full->data[BATCH_SIZE - 1] == BATCH_SIZE - 1);
    REQUIRE(full->sequence_number == 0);
    queue.pop();
    REQUIRE(queue.front());
    auto &partial = std::get<U64BatchPtr>(*queue.front());
    REQUIRE(partial->size == 1);
    REQUIRE(partial->data[0] == 42);
    REQUIRE(partial->sequence_number == 1);
    queue.pop();
    REQUIRE(queue.front());
    REQUIRE(std::holds_alternative<EndOfFile>(*queue.front()));
    queue.pop();
    REQUIRE_FALSE(queue.front());
}

TEST_CASE("Injector broadcasts EOF to every queue", "[Injector]") {
    using MessageQueue = rigtorp::SPSCQueue<QueueMessage<U64BatchPtr>>;
    Batch_Pool<U64Batch> pool(2);
    auto queues =
        make_array<MessageQueue>(std::make_index_sequence<3>{}, size_t{2});
    Injector<decltype(queues), U64BatchPtr, Batch_Pool<U64Batch>, uint64_t>
        injector(queues, pool);

    REQUIRE(injector.try_push(17));
    injector.finish();

    REQUIRE(queues[0].front());
    REQUIRE(std::get<U64BatchPtr>(*queues[0].front())->data[0] == 17);
    queues[0].pop();
    for (auto &queue : queues) {
        REQUIRE(queue.front());
        REQUIRE(std::holds_alternative<EndOfFile>(*queue.front()));
        queue.pop();
        REQUIRE_FALSE(queue.front());
    }
}
