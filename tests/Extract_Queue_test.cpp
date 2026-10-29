#include "QueueUtility/Extractor.hpp"
#include "Batch.hpp"
#include "Batch_Pool.hpp"
#include "Delay_Calc.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstdint>
#include "Utility/SPSCQueue.h"

using Queue = rigtorp::SPSCQueue<U64BatchPtr>;

namespace {

// TEST_CASE("Extract and inject with abstraction") {
//     Batch_Pool<Batch<uint64_t>> pool(128);
//     Queue in(128);
//
//     Extractor<Queue, uint64_t> ext(in);
//     Injector inj(in, pool);
//     uint64_t counter = 0;
//     for (size_t i = 0; i < BATCH_SIZE * 100; ++i) {
//         REQUIRE(inj.try_push(counter++));
//     }
//     REQUIRE(inj.try_push(counter++));
//     REQUIRE(in.size() == 100);
//
//     counter = 0;
//     for (size_t i = 0; i < BATCH_SIZE * 100; ++i) {
//         CAPTURE(i);
//         REQUIRE(ext.front());
//         REQUIRE(*ext.front() == counter++);
//         ext.pop();
//     }
// }
}  // namespace
