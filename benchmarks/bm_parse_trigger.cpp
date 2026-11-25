#include <benchmark/benchmark.h>

#include "Parsing/ParseTrigger.hpp"
#include "Utility/SPSCQueue.h"

static void BM_parse_trigger(benchmark::State& state) {
    constexpr uint64_t trigger_word = 0x6000000000000000;
    Batch_Pool<U64Batch> batch_pool(5);
    Batch_Pool<TriggerBatch> batch_pool_trigger(5);
    ParseTrigger parser(batch_pool_trigger);

    for (auto& batch : batch_pool.m_batches) {
        batch->data.fill(trigger_word);
    }

    for (auto _ : state) {
        auto input = batch_pool.acquire_batch_blocking();
        input->size = BATCH_SIZE;
        auto output = parser.parse(input);
        benchmark::DoNotOptimize(output->toa[0]);
    }
    state.SetItemsProcessed(state.iterations() * BATCH_SIZE);
}

BENCHMARK(BM_parse_trigger);