#include <benchmark/benchmark.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <print>
#include <vector>
#include "Compare.hpp"
#include "Memory_Map.hpp"
#include "Merging_Buffer.hpp"
#include "Parser.hpp"
#include "Queue_Status.hpp"
#include "Sink.hpp"
#include "Sorter.hpp"
#include "Sorting_Buffer.hpp"

using MPMC = rigtorp::MPMCQueue<U64BatchPtr>;
using SPSC = rigtorp::SPSCQueue<U64BatchPtr>;

const std::filesystem::path TESTPATH = std::filesystem::path("./tpx3.tpx3");

// Benchmark for std::sort
static void BM_Parser(benchmark::State &state) {
    Parser parser;
    ASSERT(std::filesystem::exists(TESTPATH), TESTPATH);
    MemoryMap map(TESTPATH);

    std::array<Sink<MPMC>, 4> hit_consumers;
    for (size_t i = 0; i < hit_consumers.size(); ++i) {
        hit_consumers[i].start(parser.get_batch_handle().m_hit_queue[i]);
    }
    std::array<Sink<MPMC>, 1> trigger_consumers;
    for (size_t i = 0; i < trigger_consumers.size(); ++i) {
        trigger_consumers[i].start(parser.get_batch_handle().m_trigger_queue);
    }

    for (auto _ : state) {
        parser.parse(map);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            (map.end() - map.begin()) * 8);
}

template <size_t Length>
static void BM_Parser_Sorter(benchmark::State &state) {
    Parser parser;
    ASSERT(std::filesystem::exists(TESTPATH), TESTPATH);
    MemoryMap map(TESTPATH);

    std::array<Sink<MPMC>, 4> hit_consumers;
    for (size_t i = 0; i < hit_consumers.size(); ++i) {
        hit_consumers[i].start(parser.get_batch_handle().m_hit_queue[i]);
    }

    constexpr int NUM_THREADS = 8;

    std::array<std::unique_ptr<SPSC>, NUM_THREADS> sorted_trigger_queue{};

    using SortT = Sort<MPMC, SPSC, Compare_Triggers, Length>;
    std::array<std::unique_ptr<SortT>, NUM_THREADS> trigger_sorter;
    for (int i = 0; i < NUM_THREADS; ++i) {
        sorted_trigger_queue[i] = std::make_unique<SPSC>(QUEUE_SIZE);
        trigger_sorter[i] = std::make_unique<SortT>(
            parser.get_batch_handle().m_trigger_queue, *sorted_trigger_queue[i],
            parser.get_batch_handle().batch_pool);
        trigger_sorter[i]->start_worker();
    }

    std::array<Sink<SPSC>, NUM_THREADS> trigger_consumers;
    for (size_t i = 0; i < trigger_consumers.size(); ++i) {
        trigger_consumers[i].start(*sorted_trigger_queue[i]);
    }

    for (auto _ : state) {
        parser.parse(map);
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            (map.end() - map.begin()) * 8);
}

// static void BM_Parser_Sorter_Merger(benchmark::State &state) {
//     Parser parser;
//     MemoryMap map("./tpx3.tpx3");
//
//     std::array<Hit_Consumer, 4> hit_consumers;
//     for (size_t i = 0; i < hit_consumers.size(); ++i) {
//         hit_consumers[i].start(parser.get_batch_handle().m_hit_queue[i],
//                                parser.get_batch_handle().batch_pool);
//     }
//
//     constexpr auto Num_Trigger_Sorter = 4;
//     std::array<SPSCQueue, Num_Trigger_Sorter> sorted_trigger_queue{
//         SPSCQueue{QUEUE_SIZE}, SPSCQueue{QUEUE_SIZE}, SPSCQueue{QUEUE_SIZE},
//         SPSCQueue{QUEUE_SIZE}};
//
//     std::array<std::unique_ptr<Sort<Compare_Triggers, 4>>,
//     Num_Trigger_Sorter>
//         trigger_sorter;
//     for (auto i = 0; i < Num_Trigger_Sorter; ++i) {
//         trigger_sorter[i] = std::make_unique<Sort<Compare_Triggers, 4>>(
//             parser.get_batch_handle().m_trigger_queue,
//             sorted_trigger_queue[i], parser.get_batch_handle().batch_pool);
//         trigger_sorter[i]->start_worker();
//     }
//
//     MPMCQueue merged_trigger_queue(QUEUE_SIZE);
//     Merger<Compare_Triggers, Num_Trigger_Sorter> trigger_merger(
//         std::array<SPSCQueue *, Num_Trigger_Sorter>{
//             &sorted_trigger_queue[0], &sorted_trigger_queue[1],
//             &sorted_trigger_queue[2], &sorted_trigger_queue[3]},
//         &merged_trigger_queue);
//
//     std::jthread merger_thread([&trigger_merger]() {
//         trigger_merger.startup();
//         while (true) {
//             trigger_merger.step();
//         }
//     });
//
//     std::array<Trigger_Consumer, 1> trigger_consumers;
//     for (size_t i = 0; i < trigger_consumers.size(); ++i) {
//         trigger_consumers[i].start(merged_trigger_queue,
//                                    parser.get_batch_handle().batch_pool);
//     }
//
//     for (auto _ : state) {
//         parser.parse(map);
//     }
//     for (auto &sorter : trigger_sorter) {
//         sorter->stop();
//     }
//     state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
//                             (map.end() - map.begin()) * 8);
//
//     for (auto &consumer : hit_consumers) {
//         consumer.stop();
//     }
//     for (auto &consumer : trigger_consumers) {
//         consumer.stop();
//     }
// }

BENCHMARK(BM_Parser)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<2>)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<4>)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<8>)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<16>)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<32>)->Unit(benchmark::kSecond)->MinTime(10);
// BENCHMARK(BM_Parser_Sorter_Merger)->Unit(benchmark::kSecond)->MinTime(10);

BENCHMARK_MAIN();
