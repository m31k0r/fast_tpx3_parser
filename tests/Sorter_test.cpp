#include "Sorter.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <stdexcept>
#include "Compare.hpp"
#include "SPSCQueue.h"

TEST_CASE("Construction of Sorter") {
    rigtorp::MPMCQueue<U64BatchPtr> inqueue(10);
    rigtorp::SPSCQueue<U64BatchPtr> outqueue(10);
    Batch_Pool<Batch<uint64_t>> pool(10);
    Sort<decltype(inqueue), decltype(outqueue), Compare_Triggers> sorter(
        inqueue, outqueue, pool);

    SECTION("basic") { sorter.work(); }
    SECTION("thread") { sorter.start_worker(); }
}
