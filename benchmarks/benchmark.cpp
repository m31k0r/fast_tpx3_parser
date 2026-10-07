#include <benchmark/benchmark.h>
#include <algorithm>
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

// Benchmark for std::sort
static void BM_Parser(benchmark::State &state) {
    Parser parser;
    MemoryMap map("./tpx3.tpx3");

    std::array<Hit_Consumer, 4> hit_consumers;
    for (size_t i = 0; i < hit_consumers.size(); ++i) {
        hit_consumers[i].start(parser.get_batch_handle().m_hit_queue[i],
                               parser.get_batch_handle().batch_pool);
    }
    std::array<Trigger_Consumer, 1> trigger_consumers;
    for (size_t i = 0; i < trigger_consumers.size(); ++i) {
        trigger_consumers[i].start(parser.get_batch_handle().m_trigger_queue,
                                   parser.get_batch_handle().batch_pool);
    }

    for (auto _ : state) {
        parser.parse(map);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            (map.end() - map.begin()) * 8);

    for (auto &consumer : hit_consumers) {
        consumer.stop();
    }
    for (auto &consumer : trigger_consumers) {
        consumer.stop();
    }
}

template <size_t Length>
static void BM_Parser_Sorter(benchmark::State &state) {
    Parser parser;
    MemoryMap map("./tpx3.tpx3");

    std::array<Hit_Consumer, 4> hit_consumers;
    for (size_t i = 0; i < hit_consumers.size(); ++i) {
        hit_consumers[i].start(parser.get_batch_handle().m_hit_queue[i],
                               parser.get_batch_handle().batch_pool);
    }

    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE>
        sorted_trigger_queue;

    Sort<Compare_Triggers, Length> trigger_sorter(
        parser.get_batch_handle().m_trigger_queue, sorted_trigger_queue,
        parser.get_batch_handle().batch_pool);

    trigger_sorter.start_worker();

    std::array<Trigger_Consumer, 1> trigger_consumers;
    for (size_t i = 0; i < trigger_consumers.size(); ++i) {
        trigger_consumers[i].start(sorted_trigger_queue,
                                   parser.get_batch_handle().batch_pool);
    }

    for (auto _ : state) {
        parser.parse(map);
    }
    trigger_sorter.stop();
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            (map.end() - map.begin()) * 8);

    for (auto &consumer : hit_consumers) {
        consumer.stop();
    }
    for (auto &consumer : trigger_consumers) {
        consumer.stop();
    }
}

static void BM_Parser_Sorter_Merger(benchmark::State &state) {
    Parser parser;
    MemoryMap map("./tpx3.tpx3");

    std::array<Hit_Consumer, 4> hit_consumers;
    for (size_t i = 0; i < hit_consumers.size(); ++i) {
        hit_consumers[i].start(parser.get_batch_handle().m_hit_queue[i],
                               parser.get_batch_handle().batch_pool);
    }

    constexpr auto Num_Trigger_Sorter = 4;
    std::array<es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE>,
               Num_Trigger_Sorter>
        sorted_trigger_queue;

    std::array<std::unique_ptr<Sort<Compare_Triggers, 4>>, Num_Trigger_Sorter>
        trigger_sorter;
    for (auto i = 0; i < Num_Trigger_Sorter; ++i) {
        trigger_sorter[i] = std::make_unique<Sort<Compare_Triggers, 4>>(
            parser.get_batch_handle().m_trigger_queue, sorted_trigger_queue[i],
            parser.get_batch_handle().batch_pool);
        trigger_sorter[i]->start_worker();
    }

    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE>
        merged_trigger_queue;
    Merger<Compare_Triggers, Num_Trigger_Sorter> trigger_merger(
        {&sorted_trigger_queue[0], &sorted_trigger_queue[1],
         &sorted_trigger_queue[2], &sorted_trigger_queue[3]},
        &merged_trigger_queue);

    std::jthread merger_thread([&trigger_merger]() {
        trigger_merger.startup();
        while (true) {
            trigger_merger.step();
        }
    });

    std::array<Trigger_Consumer, 1> trigger_consumers;
    for (size_t i = 0; i < trigger_consumers.size(); ++i) {
        trigger_consumers[i].start(merged_trigger_queue,
                                   parser.get_batch_handle().batch_pool);
    }

    Queue_State_Printer queue_state_printer(
        {&parser.get_batch_handle().m_hit_queue[0],
         &parser.get_batch_handle().m_hit_queue[1],
         &parser.get_batch_handle().m_hit_queue[2],
         &parser.get_batch_handle().m_hit_queue[3],
         &parser.get_batch_handle().m_trigger_queue, &sorted_trigger_queue[0],
         &sorted_trigger_queue[1], &sorted_trigger_queue[2],
         &sorted_trigger_queue[3], &merged_trigger_queue},
        {"Hit Queue 0", "Hit Queue 1", "Hit Queue 2", "Hit Queue 3",
         "Trigger Queue", "Sorted Trigger Queue 0", "Sorted Trigger Queue 1",
         "Sorted Trigger Queue 2", "Sorted Trigger Queue 3",
         "Merged Trigger Queue"});
    std::this_thread::sleep_for(std::chrono::seconds(3));
    for (auto _ : state) {
        parser.parse(map);
    }
    for (auto &sorter : trigger_sorter) {
        sorter->stop();
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            (map.end() - map.begin()) * 8);

    for (auto &consumer : hit_consumers) {
        consumer.stop();
    }
    for (auto &consumer : trigger_consumers) {
        consumer.stop();
    }
}

BENCHMARK(BM_Parser)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<2>)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<4>)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<8>)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<16>)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<32>)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter_Merger)->Unit(benchmark::kSecond)->MinTime(10);

BENCHMARK_MAIN();
