#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <thread>
#include <variant>

#include "Operations/Delay.hpp"
#include "Operations/MergeSPSC.hpp"
#include "Operations/Merging_Buffer.hpp"
#include "Operations/Sorter.hpp"
#include "Operations/Worker.hpp"
#include "Utility/helper.hpp"

namespace {
template <typename Queue>
auto wait_front(Queue& queue) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    auto* message = queue.front();
    while (not message && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
        message = queue.front();
    }
    return message;
}

using HitQueue = rigtorp::SPSCQueue<QueueMessage<HitBatchPtr>>;
using TriggerQueue = rigtorp::SPSCQueue<QueueMessage<TriggerBatchPtr>>;

struct TestTriggerWork {
    Batch_Pool<TriggerBatch>& pool;
    TriggerBatchPtr parse(U64BatchPtr input) {
        auto output = pool.acquire_batch_blocking();
        output->sequence_number = input->sequence_number;
        for (auto value : *input) output->push_back(value, 15);
        return output;
    }
};
}  // namespace

TEST_CASE("Extractor preserves EOF after buffered partial batches", "[EOF]") {
    Batch_Pool<HitBatch> pool(2);
    HitQueue queue(3);
    Extractor<HitQueue, HitBatchPtr> extractor(queue);
    auto first = pool.acquire_batch_blocking();
    first->push_back({10, 0, 0, 0});
    queue.push(std::move(first));
    auto second = pool.acquire_batch_blocking();
    second->push_back({20, 0, 0, 0});
    queue.push(std::move(second));
    queue.push(EndOfStream{});
    extractor.drain();
    REQUIRE(extractor.front().toa() == 10);
    extractor.pop();
    REQUIRE(extractor.front().toa() == 20);
    REQUIRE_FALSE(extractor.eof());
    extractor.pop();
    REQUIRE_FALSE(extractor.front());
    REQUIRE(extractor.eof());
}

TEST_CASE("Extractor preserves file boundaries between batches", "[NewFile]") {
    Batch_Pool<HitBatch> pool(2);
    HitQueue queue(3);
    Extractor<HitQueue, HitBatchPtr> extractor(queue);

    auto first = pool.acquire_batch_blocking();
    first->push_back({10, 0, 0, 0});
    queue.push(std::move(first));
    queue.push(NewFile{"next.tpx3"});
    auto second = pool.acquire_batch_blocking();
    second->push_back({20, 0, 0, 0});
    queue.push(std::move(second));

    extractor.drain();
    REQUIRE(extractor.front().toa() == 10);
    extractor.pop();
    REQUIRE_FALSE(extractor.front());
    REQUIRE(extractor.new_file());
    REQUIRE(extractor.file_message().filename == "next.tpx3");
    extractor.consume_new_file();
    REQUIRE(extractor.front().toa() == 20);
}

TEST_CASE("Trigger merger waits for every input and preserves sequence",
          "[EOF]") {
    Batch_Pool<TriggerBatch> pool(3);
    auto queues =
        make_array<TriggerQueue>(std::make_index_sequence<3>{}, size_t{3});
    SPSCs2SPSC<TriggerQueue, 3, TriggerBatchPtr> merger(queues);
    auto first = pool.acquire_batch_blocking();
    first->sequence_number = 0;
    first->push_back(10, 15);
    queues[0].push(std::move(first));
    queues[0].push(EndOfStream{});
    queues[2].push(EndOfStream{});
    REQUIRE(merger.front());
    REQUIRE(std::get<TriggerBatchPtr>(*merger.front())->toa[0] == 10);
    merger.pop();
    REQUIRE_FALSE(merger.front());
    auto second = pool.acquire_batch_blocking();
    second->sequence_number = 1;
    second->push_back(20, 15);
    queues[1].push(std::move(second));
    queues[1].push(EndOfStream{});
    REQUIRE(merger.front());
    REQUIRE(std::get<TriggerBatchPtr>(*merger.front())->toa[0] == 20);
    merger.pop();
    REQUIRE(merger.front());
    REQUIRE(std::holds_alternative<EndOfStream>(*merger.front()));
    merger.pop();
    REQUIRE_FALSE(merger.front());
}

TEST_CASE("Trigger merger forwards new-file boundaries and continues sequence",
          "[NewFile]") {
    Batch_Pool<TriggerBatch> pool(2);
    auto queues =
        make_array<TriggerQueue>(std::make_index_sequence<2>{}, size_t{4});
    SPSCs2SPSC<TriggerQueue, 2, TriggerBatchPtr> merger(queues);

    auto first = pool.acquire_batch_blocking();
    first->sequence_number = 0;
    first->push_back(10, 15);
    queues[0].push(std::move(first));
    queues[0].push(NewFile{"next.tpx3"});
    auto second = pool.acquire_batch_blocking();
    second->sequence_number = 1;
    second->push_back(20, 15);
    queues[1].push(std::move(second));
    queues[1].push(NewFile{"next.tpx3"});

    REQUIRE(merger.front());
    REQUIRE(std::get<TriggerBatchPtr>(*merger.front())->sequence_number == 0);
    merger.pop();
    REQUIRE(merger.front());
    REQUIRE(std::get<TriggerBatchPtr>(*merger.front())->sequence_number == 1);
    merger.pop();
    REQUIRE(merger.front());
    REQUIRE(std::get<NewFile>(*merger.front()).filename == "next.tpx3");
    merger.pop();

    auto third = pool.acquire_batch_blocking();
    third->sequence_number = 2;
    third->push_back(30, 15);
    queues[0].push(std::move(third));
    REQUIRE(merger.front());
    REQUIRE(std::get<TriggerBatchPtr>(*merger.front())->sequence_number == 2);
}

TEST_CASE("Hit merger waits for all inputs and emits its final batch",
          "[EOF]") {
    Batch_Pool<HitBatch> pool(8);
    auto queues =
        make_array<HitQueue>(std::make_index_sequence<4>{}, size_t{3});
    Merger merger(pool,
                  make_array_l<Extractor<HitQueue, HitBatchPtr>>(
                      std::make_index_sequence<4>{}, [&queues](size_t i) {
                          return Extractor<HitQueue, HitBatchPtr>(queues[i]);
                      }));
    auto hit = pool.acquire_batch_blocking();
    hit->push_back({10, 0, 0, 0});
    queues[0].push(std::move(hit));
    queues[0].push(EndOfStream{});
    queues[1].push(EndOfStream{});
    queues[2].push(EndOfStream{});
    REQUIRE_FALSE(merger.front());
    auto later = pool.acquire_batch_blocking();
    later->push_back({20, 0, 0, 0});
    queues[3].push(std::move(later));
    queues[3].push(EndOfStream{});
    auto* message = merger.front();
    REQUIRE(message);
    auto& batch = std::get<HitBatchPtr>(*message);
    REQUIRE(batch->size == 2);
    REQUIRE(batch->toa[0] == 10);
    REQUIRE(batch->toa[1] == 20);
    merger.pop();
    REQUIRE(merger.front());
    REQUIRE(std::holds_alternative<EndOfStream>(*merger.front()));
    merger.pop();
    REQUIRE_FALSE(merger.front());
}

TEST_CASE("Hit merger emits EOF after empty or exactly full input", "[EOF]") {
    for (bool full : {false, true}) {
        Batch_Pool<HitBatch> pool(4);
        auto queues =
            make_array<HitQueue>(std::make_index_sequence<4>{}, size_t{2});
        Merger merger(
            pool, make_array_l<Extractor<HitQueue, HitBatchPtr>>(
                      std::make_index_sequence<4>{}, [&queues](size_t i) {
                          return Extractor<HitQueue, HitBatchPtr>(queues[i]);
                      }));
        if (full) {
            auto batch = pool.acquire_batch_blocking();
            for (size_t i = 0; i < BATCH_SIZE; ++i) {
                batch->push_back({i, 0, 0, 0});
            }
            queues[0].push(std::move(batch));
        }
        for (auto& queue : queues) queue.push(EndOfStream{});
        if (full) {
            REQUIRE(merger.front());
            REQUIRE(std::get<HitBatchPtr>(*merger.front())->size == BATCH_SIZE);
            merger.pop();
        }
        REQUIRE(merger.front());
        REQUIRE(std::holds_alternative<EndOfStream>(*merger.front()));
        merger.pop();
        REQUIRE_FALSE(merger.front());
    }
}

TEST_CASE("Delay flushes hits after trigger EOF and then emits EOF", "[EOF]") {
    Batch_Pool<HitBatch> pool(4);
    Batch_Pool<TriggerBatch> trigger_pool(1);
    HitQueue hits(2);
    TriggerQueue triggers(2);
    HitQueue output(2);
    Camera camera_;
    auto hit = pool.acquire_batch_blocking();
    hit->push_back({100, 1, 2, 3});
    hit->push_back({200, 1, 2, 3});
    hits.push(std::move(hit));
    hits.push(EndOfStream{});
    auto trigger = trigger_pool.acquire_batch_blocking();
    trigger->push_back(50, 15);
    triggers.push(std::move(trigger));
    triggers.push(EndOfStream{});
    {
        Delay<HitQueue, TriggerQueue, HitQueue> delay(hits, triggers, output,
                                                      pool, camera_);
        auto* message = wait_front(output);
        REQUIRE(message);
        auto& batch = std::get<HitBatchPtr>(*message);
        REQUIRE(batch->size == 2);
        REQUIRE(batch->toa[0] == 100);
        REQUIRE(batch->toa[1] == 200);
        REQUIRE(batch->delay[0] == 50);
        REQUIRE(batch->delay[1] == 150);
        output.pop();
        message = wait_front(output);
        REQUIRE(message);
        REQUIRE(std::holds_alternative<EndOfStream>(*message));
        output.pop();
    }
}

TEST_CASE("Delay processes hits without triggers", "[EOF]") {
    Batch_Pool<HitBatch> pool(3);
    HitQueue hits(2);
    TriggerQueue triggers(1);
    HitQueue output(2);
    Camera camera_;
    auto hit = pool.acquire_batch_blocking();
    hit->push_back({42, 0, 0, 0});
    hits.push(std::move(hit));
    hits.push(EndOfStream{});
    triggers.push(EndOfStream{});
    Delay<HitQueue, TriggerQueue, HitQueue> delay(hits, triggers, output, pool,
                                                  camera_);
    auto* message = wait_front(output);
    REQUIRE(message);
    REQUIRE(std::get<HitBatchPtr>(*message)->delay[0] == 0);
    output.pop();
    message = wait_front(output);
    REQUIRE(message);
    REQUIRE(std::holds_alternative<EndOfStream>(*message));
    output.pop();
}

TEST_CASE("New-file boundaries travel through hit merge and delay",
          "[NewFile]") {
    Batch_Pool<HitBatch> pool(8);
    auto hits =
        make_array<HitQueue>(std::make_index_sequence<4>{}, size_t{4});
    TriggerQueue triggers(2);
    HitQueue output(4);
    auto first = pool.acquire_batch_blocking();
    first->push_back({10, 1, 2, 3});
    hits[0].push(std::move(first));
    hits[0].push(NewFile{"next.tpx3"});
    auto second = pool.acquire_batch_blocking();
    second->push_back({20, 4, 5, 6});
    hits[0].push(std::move(second));
    hits[0].push(EndOfStream{});
    for (size_t i = 1; i < hits.size(); ++i) {
        hits[i].push(NewFile{"next.tpx3"});
        hits[i].push(EndOfStream{});
    }

    Merger hit_merger(
        pool, make_array_l<Extractor<HitQueue, HitBatchPtr>>(
                  std::make_index_sequence<4>{}, [&hits](size_t i) {
                      return Extractor<HitQueue, HitBatchPtr>(hits[i]);
                  }));
    triggers.push(NewFile{"next.tpx3"});
    triggers.push(EndOfStream{});
    Camera camera;
    {
        Delay<Merger, TriggerQueue, HitQueue> delay(hit_merger, triggers,
                                                    output, pool, camera);
        auto* message = wait_front(output);
        REQUIRE(message);
        REQUIRE(std::get<HitBatchPtr>(*message)->toa[0] == 10);
        output.pop();
        message = wait_front(output);
        REQUIRE(message);
        REQUIRE(std::get<NewFile>(*message).filename == "next.tpx3");
        output.pop();
        message = wait_front(output);
        REQUIRE(message);
        REQUIRE(std::get<HitBatchPtr>(*message)->toa[0] == 20);
        output.pop();
        message = wait_front(output);
        REQUIRE(message);
        REQUIRE(std::holds_alternative<EndOfStream>(*message));
        output.pop();
    }
}

TEST_CASE("Sorter emits a retained full batch before EOF", "[EOF]") {
    using RawQueue = rigtorp::SPSCQueue<QueueMessage<U64BatchPtr>>;
    Batch_Pool<U64Batch> raw_pool(1);
    Batch_Pool<HitBatch> hit_pool(1);
    RawQueue input(2);
    HitQueue output(2);
    Camera camera;
    auto sorter = std::make_unique<Sort<RawQueue, HitQueue>>(input, output,
                                                             hit_pool, camera);
    auto raw = raw_pool.acquire_batch_blocking();
    for (size_t i = 0; i < BATCH_SIZE; ++i) raw->push_back(0);
    input.push(std::move(raw));
    input.push(EndOfStream{});
    sorter->start_worker();
    auto* message = wait_front(output);
    REQUIRE(message);
    REQUIRE(std::get<HitBatchPtr>(*message)->size == BATCH_SIZE);
    output.pop();
    message = wait_front(output);
    REQUIRE(message);
    REQUIRE(std::holds_alternative<EndOfStream>(*message));
    output.pop();
}

TEST_CASE("EOF travels through the complete pipeline", "[EOF]") {
    using RawQueue = rigtorp::SPSCQueue<QueueMessage<U64BatchPtr>>;
    Batch_Pool<U64Batch> raw_pool(4);
    Batch_Pool<HitBatch> hit_pool(12);
    Batch_Pool<TriggerBatch> trigger_pool(2);
    auto raw_hits =
        make_array<RawQueue>(std::make_index_sequence<4>{}, size_t{2});
    auto sorted =
        make_array<HitQueue>(std::make_index_sequence<4>{}, size_t{2});
    auto raw_triggers =
        make_array<RawQueue>(std::make_index_sequence<1>{}, size_t{2});
    auto parsed =
        make_array<TriggerQueue>(std::make_index_sequence<1>{}, size_t{2});
    HitQueue output(2);
    Camera camera;
    Merger hit_merger(
        hit_pool, make_array_l<Extractor<HitQueue, HitBatchPtr>>(
                      std::make_index_sequence<4>{}, [&sorted](size_t i) {
                          return Extractor<HitQueue, HitBatchPtr>(sorted[i]);
                      }));
    SPSCs2SPSC<TriggerQueue, 1, TriggerBatchPtr> trigger_merger(parsed);
    std::array<std::unique_ptr<Sort<RawQueue, HitQueue>>, 4> sorters;
    for (size_t i = 0; i < 4; ++i) {
        sorters[i] = std::make_unique<Sort<RawQueue, HitQueue>>(
            raw_hits[i], sorted[i], hit_pool, camera, static_cast<uint8_t>(i));
        if (i < 2) {
            auto batch = raw_pool.acquire_batch_blocking();
            batch->push_back(i + 1);
            raw_hits[i].push(std::move(batch));
        }
        raw_hits[i].push(EndOfStream{});
        sorters[i]->start_worker();
    }
    auto trigger = raw_pool.acquire_batch_blocking();
    trigger->sequence_number = 0;
    trigger->push_back(0);
    raw_triggers[0].push(std::move(trigger));
    raw_triggers[0].push(EndOfStream{});
    Worker<TestTriggerWork, RawQueue, TriggerQueue> worker(
        TestTriggerWork{trigger_pool}, raw_triggers[0], parsed[0]);
    Delay<Merger, decltype(trigger_merger), HitQueue> delay(
        hit_merger, trigger_merger, output, hit_pool, camera);
    auto* message = wait_front(output);
    REQUIRE(message);
    auto& batch = std::get<HitBatchPtr>(*message);
    REQUIRE(batch->size == 2);
    REQUIRE(batch->toa[0] <= batch->toa[1]);
    output.pop();
    message = wait_front(output);
    REQUIRE(message);
    REQUIRE(std::holds_alternative<EndOfStream>(*message));
    output.pop();
}
