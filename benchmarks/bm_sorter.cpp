#include <benchmark/benchmark.h>

#include <array>
#include <utility>

#include "Operations/Sorter.hpp"
#include "Utility/SPSCQueue.h"
#include "Utility/helper.hpp"

namespace {

struct BenchmarkCamera {
    uint64_t get_row_correction(uint8_t, uint16_t) { return 0; }

    std::pair<uint16_t, uint16_t> fill_gap(
        std::pair<uint16_t, uint16_t> position, uint8_t) {
        return position;
    }

    std::pair<uint16_t, uint16_t> to_global(
        std::pair<uint16_t, uint16_t> position, uint8_t) {
        return position;
    }
};

using RawQueue = rigtorp::SPSCQueue<QueueMessage<U64BatchPtr>>;
using SortedQueue = rigtorp::SPSCQueue<QueueMessage<HitBatchPtr>>;

void push_batch(RawQueue& queue, Batch_Pool<U64Batch>& pool) {
    auto batch = pool.acquire_batch_blocking();
    for (size_t i = 0; i < BATCH_SIZE; ++i) {
        batch->push_back(0);
    }
    while (not queue.try_push(std::move(batch))) {
    }
}

void wait_for_sorted_batch(SortedQueue& queue) {
    for (;;) {
        auto* message = queue.front();
        if (message == nullptr) continue;
        if (std::holds_alternative<HitBatchPtr>(*message)) {
            benchmark::DoNotOptimize(std::get<HitBatchPtr>(*message)->size);
            queue.pop();
            return;
        }
        queue.pop();
    }
}

}  // namespace

static void BM_sorter_thread(benchmark::State& state) {
    constexpr size_t sorter_count = 4;
    using Sorter = Sort<RawQueue, SortedQueue, BenchmarkCamera>;
    Batch_Pool<U64Batch> raw_pool(8);
    Batch_Pool<HitBatch> hit_pool(8);
    auto inputs = make_array<RawQueue>(
        std::make_index_sequence<sorter_count>{}, size_t{4});
    auto outputs = make_array<SortedQueue>(
        std::make_index_sequence<sorter_count>{}, size_t{4});
    BenchmarkCamera camera;
    auto sorters = make_array_l<Sorter>(
        std::make_index_sequence<sorter_count>{}, [&](size_t i) {
            return Sorter(inputs[i], outputs[i], hit_pool, camera,
                          static_cast<uint8_t>(i));
        });
    for (auto& sorter : sorters) {
        sorter.start_worker();
    }

    // The sorter retains one batch as its ordering window.
    for (auto& input : inputs) {
        push_batch(input, raw_pool);
    }

    for (auto _ : state) {
        for (auto& input : inputs) {
            push_batch(input, raw_pool);
        }
        for (auto& output : outputs) {
            wait_for_sorted_batch(output);
        }
    }

    for (auto& input : inputs) {
        while (not input.try_push(EndOfStream{})) {
        }
    }
    for (auto& output : outputs) {
        for (;;) {
            auto* message = output.front();
            if (message == nullptr) continue;
            const bool finished = std::holds_alternative<EndOfStream>(*message);
            output.pop();
            if (finished) break;
        }
    }

    state.SetItemsProcessed(state.iterations() * BATCH_SIZE * sorter_count);
}

BENCHMARK(BM_sorter_thread);
