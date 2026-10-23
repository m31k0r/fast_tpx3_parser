

#include "Delay_Calc.hpp"
#include "Batch.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstdint>
#include "Batch_Pool.hpp"
#include "Extract_Queue.hpp"
#include "SPSCQueue.h"

using Queue = rigtorp::SPSCQueue<U64BatchPtr>;

namespace {
TEST_CASE("Delay Queue") {
    Queue trigger(128);
    Queue hit(128);
    Queue out(128);
    Batch_Pool<Batch<uint64_t>> pool(128);

    auto hit_q = Extractor<Queue, uint64_t>(hit);
    auto trigger_q = Extractor<Queue, uint64_t>(trigger);
    auto out_q = Injector<Queue, uint64_t>(out, pool);

    Delay<Extractor<Queue, uint64_t>, Injector<Queue, uint64_t>> delay(
        hit_q, trigger_q, out_q);
    // REQUIRE(not delay.startup());
}
}  // namespace
