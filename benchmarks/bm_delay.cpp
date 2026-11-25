#include <benchmark/benchmark.h>

#include <limits>

#include "Operations/Delay.hpp"
#include "Utility/SPSCQueue.h"

namespace {

using HitQueue = rigtorp::SPSCQueue<QueueMessage<HitBatchPtr>>;
using TriggerQueue = rigtorp::SPSCQueue<QueueMessage<TriggerBatchPtr>>;

void push_hit_message(HitQueue& queue, QueueMessage<HitBatchPtr> message) {
    while (not queue.try_push(std::move(message))) {
    }
}

void push_trigger_message(TriggerQueue& queue,
                          QueueMessage<TriggerBatchPtr> message) {
    while (not queue.try_push(std::move(message))) {
    }
}

}  // namespace

static void BM_delay_thread(benchmark::State& state) {
    constexpr size_t hits_per_iteration = 1024;
    constexpr uint16_t delay_trigger_type = 0xF;
    constexpr uint16_t diff_trigger_type = 0xb;
    Batch_Pool<HitBatch> hit_pool(8);
    Batch_Pool<TriggerBatch> trigger_pool(2);
    HitQueue hits(4);
    TriggerQueue triggers(2);
    HitQueue output(4);

    {
        Delay<HitQueue, TriggerQueue, HitQueue> delay(
            hits, triggers, output, hit_pool, diff_trigger_type,
            delay_trigger_type);

        for (auto _ : state) {
            auto hit_batch = hit_pool.acquire_batch_blocking();
            for (size_t i = 0; i < hits_per_iteration; ++i) {
                hit_batch->push_back({static_cast<uint64_t>(i), 1, 2, 3});
            }
            push_hit_message(
                hits, QueueMessage<HitBatchPtr>(std::move(hit_batch)));
            push_hit_message(hits, QueueMessage<HitBatchPtr>(Flush{}));

            auto trigger_batch = trigger_pool.acquire_batch_blocking();
            trigger_batch->push_back(std::numeric_limits<uint64_t>::max(),
                                      static_cast<uint8_t>(delay_trigger_type));
            push_trigger_message(
                triggers,
                QueueMessage<TriggerBatchPtr>(std::move(trigger_batch)));
            push_trigger_message(triggers,
                                 QueueMessage<TriggerBatchPtr>(Flush{}));

            for (;;) {
                auto* message = output.front();
                if (message == nullptr) continue;
                if (std::holds_alternative<HitBatchPtr>(*message)) {
                    benchmark::DoNotOptimize(
                        std::get<HitBatchPtr>(*message)->size);
                }
                const bool flushed = std::holds_alternative<Flush>(*message);
                output.pop();
                if (flushed) break;
            }
        }

        push_hit_message(hits, QueueMessage<HitBatchPtr>(EndOfStream{}));
        push_trigger_message(triggers,
                             QueueMessage<TriggerBatchPtr>(EndOfStream{}));
        for (;;) {
            auto* message = output.front();
            if (message == nullptr) continue;
            const bool finished = std::holds_alternative<EndOfStream>(*message);
            output.pop();
            if (finished) break;
        }
    }

    state.SetItemsProcessed(state.iterations() * hits_per_iteration);
}

BENCHMARK(BM_delay_thread);
