#include <catch2/catch_test_macros.hpp>

#include "QueueUtility/Injector.hpp"
#include "Batch.hpp"
#include "../Batch_Pool.hpp"
#include "../Constants.hpp"
#include "Utility/SPSCQueue.h"

struct U64Batch;
using SPSC = rigtorp::SPSCQueue<U64BatchPtr>;

TEST_CASE("Injector", "[Injector]") {
    Batch_Pool<U64Batch> batch_pool(100);
    SPSC spsc(100);
    uint64_t counter = 1;
    Injector<SPSC, U64BatchPtr, Batch_Pool<U64Batch>, uint64_t> injector(spsc, batch_pool);
    for (size_t i = 0; i < 100 * BATCH_SIZE; i++) {
        REQUIRE(injector.try_push(counter++));
    }
    counter=1;
    for (size_t i = 0; i < 99;++i) {
        U64BatchPtr *batch = spsc.front();
        REQUIRE(batch);
        REQUIRE(*batch);
        for (auto &b: **batch) {
            REQUIRE(b==counter++);
        }
        spsc.pop();
    }

}