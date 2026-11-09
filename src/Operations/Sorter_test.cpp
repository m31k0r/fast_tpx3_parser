#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

#include "Operations/Sorter.hpp"
#include "Utility/SPSCQueue.h"

namespace {
using InputQueue = rigtorp::SPSCQueue<QueueMessage<U64BatchPtr>>;
using OutputQueue = rigtorp::SPSCQueue<QueueMessage<HitBatchPtr>>;

uint64_t make_hit_data(uint16_t toa) {
    return (uint64_t{HIT_NIBBLE} << 60) | (uint64_t{toa} << 30);
}

template <typename Queue>
auto wait_for_message(Queue& queue) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    auto* message = queue.front();
    while (not message && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
        message = queue.front();
    }
    return message;
}

std::vector<uint64_t> collect_toas(OutputQueue& output, size_t count) {
    std::vector<uint64_t> toas;
    for (size_t i = 0; i < count; ++i) {
        auto* message = wait_for_message(output);
        REQUIRE(message);
        auto& batch = std::get<HitBatchPtr>(*message);
        toas.insert(toas.end(), batch->toa.begin(),
                    batch->toa.begin() + batch->size);
        output.pop();
    }
    return toas;
}
}  // namespace

TEST_CASE("Sorter sorts a partial input batch and forwards EOF", "[Sorter]") {
    Batch_Pool<U64Batch> input_pool(1);
    Batch_Pool<HitBatch> output_pool(1);
    InputQueue input(2);
    OutputQueue output(2);
    Camera camera;
    Sort<InputQueue, OutputQueue> sorter(input, output, output_pool, camera);

    auto batch = input_pool.acquire_batch_blocking();
    batch->push_back(make_hit_data(3));
    batch->push_back(make_hit_data(1));
    batch->push_back(make_hit_data(2));
    input.push(std::move(batch));
    input.push(EndOfFile{});
    sorter.start_worker();

    const auto toas = collect_toas(output, 1);
    REQUIRE(toas.size() == 3);
    REQUIRE(std::ranges::is_sorted(toas));
    auto* message = wait_for_message(output);
    REQUIRE(message);
    REQUIRE(std::holds_alternative<EndOfFile>(*message));
    output.pop();
}

TEST_CASE("Sorter merges a full batch with trailing partial hits", "[Sorter]") {
    Batch_Pool<U64Batch> input_pool(2);
    Batch_Pool<HitBatch> output_pool(2);
    InputQueue input(3);
    OutputQueue output(3);
    Camera camera;
    Sort<InputQueue, OutputQueue> sorter(input, output, output_pool, camera);

    auto full = input_pool.acquire_batch_blocking();
    for (size_t i = BATCH_SIZE; i > 0; --i) {
        full->push_back(make_hit_data(static_cast<uint16_t>(i)));
    }
    input.push(std::move(full));
    auto trailing = input_pool.acquire_batch_blocking();
    trailing->push_back(make_hit_data(0));
    trailing->push_back(make_hit_data(1));
    input.push(std::move(trailing));
    input.push(EndOfFile{});
    sorter.start_worker();

    auto toas = collect_toas(output, 2);
    REQUIRE(toas.size() == BATCH_SIZE + 2);
    REQUIRE(std::ranges::is_sorted(toas));
    auto* message = wait_for_message(output);
    REQUIRE(message);
    REQUIRE(std::holds_alternative<EndOfFile>(*message));
    output.pop();
}
