//
// Created by jholder on 29.10.26.
//
#include "QueueUtility/Extractor.hpp"

#include <catch2/catch_test_macros.hpp>

#include "../Batch/Batch.hpp"
#include "Utility/SPSCQueue.h"

using SPSC = rigtorp::SPSCQueue<U64BatchPtr>;
TEST_CASE("Extractor", "[Extractor]") {
    SPSC spsc(100);
    Batch_Pool<U64Batch> batch_pool(100);
    uint64_t counter = 1;
    for (size_t i = 0; i < 100; i++) {
        auto batch = batch_pool.acquire_batch_blocking();
        REQUIRE(batch);
        for (size_t i = 0; i < BATCH_SIZE; i++) {
            batch->push_back(counter++);
        }
        spsc.push(std::move(batch));
    }

    REQUIRE(spsc.front());
    counter = 1;
    for (auto &b : *spsc.front()->get()) {
        REQUIRE(b == counter++);
    }
    Extractor<SPSC, U64BatchPtr> extractor(spsc);
    counter = 1;
    while (extractor.front()) {
        auto batch = extractor.front();
        REQUIRE(batch);
        const auto val = *batch;

        extractor.pop();
        REQUIRE(counter == val);
        counter++;
    }
}

TEST_CASE("Extractor returns copied batches to the pool", "[Extractor]") {
    Batch_Pool<U64Batch> pool(1);
    SPSC queue(1);
    Extractor<SPSC, U64BatchPtr> extractor(queue);

    auto enqueue = [&](uint64_t value) {
        REQUIRE(pool.available_batches() == 1);
        auto batch = pool.acquire_batch_blocking();
        batch->push_back(value);
        batch->push_back(value + 10);
        queue.push(std::move(batch));
    };

    enqueue(1);
    extractor.drain();
    REQUIRE(*extractor.front() == 1);
    REQUIRE(pool.available_batches() == 1);

    for (uint64_t value = 2; value <= 4; ++value) {
        enqueue(value);
        extractor.drain();
        REQUIRE(pool.available_batches() == 1);
    }
    enqueue(5);
    REQUIRE(extractor.size() == 4);

    for (uint64_t value = 1; value <= 5; ++value) {
        REQUIRE(extractor.front());
        REQUIRE(*extractor.front() == value);
        extractor.pop();
        REQUIRE(extractor.front());
        REQUIRE(*extractor.front() == value + 10);
        extractor.pop();
    }
    REQUIRE_FALSE(extractor.front());
    REQUIRE(pool.available_batches() == 1);
}

// TEST_CASE("Extractor copies hit and trigger batches", "[Extractor]") {
//     SECTION("Hits") {
//         Batch_Pool<HitBatch> pool(1);
//         rigtorp::SPSCQueue<HitBatchPtr> queue(1);
//         Extractor<decltype(queue), HitBatchPtr> extractor(queue);
//
//         auto batch = pool.acquire_batch_blocking();
//         batch->push_back({11, 22, 33, 44});
//         queue.push(std::move(batch));
//         extractor.drain();
//         auto hit = extractor.front();
//         REQUIRE(hit);
//         REQUIRE(pool.available_batches() == 1);
//
//         batch = pool.acquire_batch_blocking();
//         batch->push_back({55, 66, 77, 88});
//         queue.push(std::move(batch));
//         extractor.drain();
//         REQUIRE(pool.available_batches() == 1);
//         REQUIRE(hit.toa() == 11);
//         REQUIRE(hit.tot() == 22);
//         REQUIRE(hit.x() == 33);
//         REQUIRE(hit.y() == 44);
//         extractor.pop();
//         hit = extractor.front();
//         REQUIRE(hit);
//         REQUIRE(hit.toa() == 55);
//         REQUIRE(hit.tot() == 66);
//         REQUIRE(hit.x() == 77);
//         REQUIRE(hit.y() == 88);
//         extractor.pop();
//         REQUIRE_FALSE(extractor.front());
//     }
//
//     SECTION("Triggers") {
//         Batch_Pool<TriggerBatch> pool(1);
//         rigtorp::SPSCQueue<TriggerBatchPtr> queue(1);
//         Extractor<decltype(queue), TriggerBatchPtr> extractor(queue);
//
//         auto batch = pool.acquire_batch_blocking();
//         batch->push_back(11, 2);
//         queue.push(std::move(batch));
//         auto trigger = extractor.front();
//         REQUIRE(trigger);
//         REQUIRE(pool.available_batches() == 1);
//
//         batch = pool.acquire_batch_blocking();
//         batch->push_back(33, 4);
//         queue.push(std::move(batch));
//         extractor.drain();
//         REQUIRE(pool.available_batches() == 1);
//         REQUIRE(trigger.toa() == 11);
//         REQUIRE(trigger.type() == 2);
//         extractor.pop();
//         trigger = extractor.front();
//         REQUIRE(trigger);
//         REQUIRE(trigger.toa() == 33);
//         REQUIRE(trigger.type() == 4);
//         extractor.pop();
//         REQUIRE_FALSE(extractor.front());
//     }
// }
