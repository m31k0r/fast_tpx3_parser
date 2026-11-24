#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

#include "Operations/Worker.hpp"
#include "Utility/SPSCQueue.h"

namespace {
struct IdentityWork {
    U64BatchPtr parse(U64BatchPtr batch) { return batch; }
};
}

TEST_CASE("Worker processes batches before stopping on EOF", "[Worker]") {
    using Input = rigtorp::SPSCQueue<QueueMessage<U64BatchPtr>>;
    using Output = rigtorp::SPSCQueue<QueueMessage<U64BatchPtr>>;
    Batch_Pool<U64Batch> pool(2);
    Input input(3);
    Output output(2);
    auto first = pool.acquire_batch_blocking();
    first->push_back(17);
    input.push(std::move(first));
    input.push(EndOfStream{});
    auto after_eof = pool.acquire_batch_blocking();
    after_eof->push_back(42);
    input.push(std::move(after_eof));

    {
        Worker<IdentityWork, Input, Output> worker(IdentityWork{}, input,
                                                   output);
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while ((output.size() != 2 || input.size() != 1) &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        REQUIRE(output.size() == 2);
        REQUIRE(input.size() == 1);
    }

    REQUIRE(std::get<U64BatchPtr>(*output.front())->data[0] == 17);
    output.pop();
    REQUIRE(std::holds_alternative<EndOfStream>(*output.front()));
    output.pop();
    REQUIRE(input.front());
    REQUIRE(std::get<U64BatchPtr>(*input.front())->data[0] == 42);
    input.pop();
}

TEST_CASE("Worker forwards new-file boundaries and keeps processing",
          "[Worker]") {
    using Queue = rigtorp::SPSCQueue<QueueMessage<U64BatchPtr>>;
    Batch_Pool<U64Batch> pool(2);
    Queue input(8);
    Queue output(8);

    auto first = pool.acquire_batch_blocking();
    first->push_back(17);
    input.push(std::move(first));
    input.push(NewFile{"next.tpx3"});
    input.push(Flush{});
    auto second = pool.acquire_batch_blocking();
    second->push_back(42);
    input.push(std::move(second));
    input.push(EndOfStream{});

    {
        Worker<IdentityWork, Queue, Queue> worker(IdentityWork{}, input,
                                                  output);
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (output.size() != 5 &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        REQUIRE(output.size() == 5);
    }

    REQUIRE(std::get<U64BatchPtr>(*output.front())->data[0] == 17);
    output.pop();
    REQUIRE(std::get<NewFile>(*output.front()).filename == "next.tpx3");
    output.pop();
    REQUIRE(std::holds_alternative<Flush>(*output.front()));
    output.pop();
    REQUIRE(std::get<U64BatchPtr>(*output.front())->data[0] == 42);
    output.pop();
    REQUIRE(std::holds_alternative<EndOfStream>(*output.front()));
}
