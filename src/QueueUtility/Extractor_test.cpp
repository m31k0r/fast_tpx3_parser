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
        for (size_t i = 0;i < BATCH_SIZE; i++) {
            batch->push_back(counter++);
        }
        spsc.push(std::move(batch));
    }

    REQUIRE(spsc.front());
    counter = 1;
    for (auto &b: *spsc.front()->get()) {
        REQUIRE(b==counter++);
    }
    Extractor<SPSC, U64BatchPtr> extractor(spsc);
    counter = 1;
    while (extractor.front()) {
        auto batch = extractor.front();
        REQUIRE(batch);
        const auto val = *batch;

        extractor.pop();
        REQUIRE(counter==val);
        counter++;
    }
}