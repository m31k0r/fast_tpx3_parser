#include <benchmark/benchmark.h>

#include "Parsing/ParseTrigger.hpp"
#include "Utility/SPSCQueue.h"

static void BM_parse_trigger(benchmark::State& state) {
    Batch_Pool<U64Batch> batch_pool(5);
    Batch_Pool<TriggerBatch> batch_pool_trigger(5);
    ParseTrigger parser(batch_pool_trigger);

    for (auto &batch: batch_pool.m_batches) {
        for (auto &b: *batch) {
            b = 0x1234;
        }
    }

    rigtorp::SPSCQueue<U64BatchPtr> spsc(100);
    for (auto _ : state) {
        auto batch = parser.parse(batch_pool.acquire_batch_blocking());
        benchmark::DoNotOptimize(batch);
    }
    state.SetItemsProcessed(state.iterations()*BATCH_SIZE);
}

BENCHMARK(BM_parse_trigger);