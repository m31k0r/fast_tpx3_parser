#include <benchmark/benchmark.h>

#include "Operations/Sorter.hpp"
#include "Utility/SPSCQueue.h"


static void BM_sorter(benchmark::State& state) {
    Batch_Pool<U64Batch> batch_pool(5);
    Batch_Pool<HitBatch> batch_pool_hits(5);


    for (auto &batch: batch_pool.m_batches) {
        for (auto &b: *batch) {
            b = 0x1234;
        }
    }

    rigtorp::SPSCQueue<U64BatchPtr> spsc(100);
    for (auto _ : state) {

    }
    state.SetItemsProcessed(state.iterations()*BATCH_SIZE);
}

BENCHMARK(BM_sorter);