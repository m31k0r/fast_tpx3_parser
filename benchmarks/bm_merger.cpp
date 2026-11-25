#include <benchmark/benchmark.h>

#include <array>
#include <thread>
#include <utility>

#include "Operations/MergeSPSC.hpp"
#include "Operations/Merging_Buffer.hpp"
#include "QueueUtility/Extractor.hpp"
#include "Utility/SPSCQueue.h"
#include "Utility/helper.hpp"

namespace {
    using HitQueue = rigtorp::SPSCQueue<QueueMessage<HitBatchPtr> >;
    using TriggerQueue = rigtorp::SPSCQueue<QueueMessage<TriggerBatchPtr> >;
} // namespace

static void BM_trigger_merger(benchmark::State &state) {
    constexpr size_t thread_count = 4;
    Batch_Pool<TriggerBatch> pool(4);
    auto queues = make_array<TriggerQueue>(
        std::make_index_sequence<thread_count>{}, size_t{2});
    SPSCs2SPSC<TriggerQueue, thread_count, TriggerBatchPtr> merger(queues);
    size_t sequence_number = 0;


    for (auto _: state) {
        const auto queue_index = sequence_number % thread_count;
        auto batch = pool.acquire_batch_blocking();
        batch->sequence_number = sequence_number++;
        batch->push_back(100, 0);
        while (not queues[queue_index].try_push(
            QueueMessage<TriggerBatchPtr>(std::move(batch)))) {
        }

        auto *message = merger.front();
        while (message == nullptr) message = merger.front();
        benchmark::DoNotOptimize(std::get<TriggerBatchPtr>(*message)->toa[0]);
        merger.pop();
    }

    state.SetItemsProcessed(state.iterations() * BATCH_SIZE);
}

/*
static void BM_hit_merger(benchmark::State &state) {
    constexpr size_t hits_per_chip = BATCH_SIZE / 4;
    Batch_Pool<HitBatch> pool(16);
    auto queues = make_array<HitQueue>(
        std::make_index_sequence<4>{}, size_t{2});
    Merger merger(
        pool, Merger::Array{
            Extractor<HitQueue, HitBatchPtr>(queues[0]),
            Extractor<HitQueue, HitBatchPtr>(queues[1]),
            Extractor<HitQueue, HitBatchPtr>(queues[2]),
            Extractor<HitQueue, HitBatchPtr>(queues[3])
        });

    std::jthread source([&queues, &pool](std::stop_token stop) {
            while (not stop.stop_requested()) {
                for (size_t chip = 0; chip < queues.size(); ++chip) {
                    auto batch = pool.acquire_batch_blocking();
                    for (size_t i = 0; i < hits_per_chip; ++i) {
                        batch->push_back(
                            {
                                static_cast<uint64_t>(i * 4 + chip), 1,
                                static_cast<uint16_t>(chip), 0
                            });
                    }
                    while (not queues[chip].try_push(
                        QueueMessage<HitBatchPtr>(std::move(batch)))) {
                    }
                }
            }
        }
    );
    for (auto _: state) {
        auto *message = merger.front();
        while (message == nullptr) {
            message = merger.front();
        }
        benchmark::DoNotOptimize(std::get<HitBatchPtr>(*message)->size);
        merger.pop();
    }

    state.SetItemsProcessed(state.iterations() * BATCH_SIZE);
}
*/

BENCHMARK(BM_trigger_merger);
//BENCHMARK(BM_hit_merger);
