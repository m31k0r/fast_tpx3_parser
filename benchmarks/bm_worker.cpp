#include <benchmark/benchmark.h>

#include "Operations/Worker.hpp"
#include "Parsing/ParseTrigger.hpp"
#include "Utility/SPSCQueue.h"

namespace {

using RawQueue = rigtorp::SPSCQueue<QueueMessage<U64BatchPtr>>;
using TriggerQueue = rigtorp::SPSCQueue<QueueMessage<TriggerBatchPtr>>;

void push_message(RawQueue& queue, QueueMessage<U64BatchPtr> message) {
    while (not queue.try_push(std::move(message))) {
    }
}

}  // namespace

static void BM_trigger_worker_thread(benchmark::State& state) {
    Batch_Pool<U64Batch> raw_pool(4);
    Batch_Pool<TriggerBatch> trigger_pool(4);
    RawQueue input(4);
    TriggerQueue output(4);
    Worker<ParseTrigger, RawQueue, TriggerQueue> worker(
        ParseTrigger(trigger_pool), input, output);
    size_t sequence_number = 0;

    for (auto _ : state) {
        auto batch = raw_pool.acquire_batch_blocking();
        batch->sequence_number = sequence_number++;
        for (size_t i = 0; i < BATCH_SIZE; ++i) {
            batch->push_back(0x6000000000000000);
        }
        push_message(input, QueueMessage<U64BatchPtr>(std::move(batch)));

        for (;;) {
            auto* message = output.front();
            if (message == nullptr) continue;
            if (std::holds_alternative<TriggerBatchPtr>(*message)) {
                benchmark::DoNotOptimize(
                    std::get<TriggerBatchPtr>(*message)->size);
                output.pop();
                break;
            }
            output.pop();
        }
    }

    push_message(input, QueueMessage<U64BatchPtr>(EndOfStream{}));
    while (output.front() == nullptr) {
    }
    output.pop();
    state.SetItemsProcessed(state.iterations() * BATCH_SIZE);
}

BENCHMARK(BM_trigger_worker_thread);
