#include <benchmark/benchmark.h>

#include "Operations/Sink.hpp"
#include "Utility/SPSCQueue.h"

static void BM_sink_thread(benchmark::State& state) {
    Batch_Pool<HitBatch> pool(8);
    rigtorp::SPSCQueue<QueueMessage<HitBatchPtr>> queue(4);
    Sink<decltype(queue), HitBatchPtr> sink;
    sink.start(queue);

    for (auto _ : state) {
        auto batch = pool.acquire_batch_blocking();
        batch->size = 1;
        while (not queue.try_push(
            QueueMessage<HitBatchPtr>(std::move(batch)))) {
        }
    }

    while (not queue.try_push(QueueMessage<HitBatchPtr>(EndOfStream{}))) {
    }
    sink.wait();
    state.SetItemsProcessed(state.iterations()*BATCH_SIZE);
}

BENCHMARK(BM_sink_thread);
