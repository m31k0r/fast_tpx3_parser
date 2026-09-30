#include <benchmark/benchmark.h>
#include <algorithm>
#include <vector>
#include "Compare.hpp"
#include "Memory_Map.hpp"
#include "Parser.hpp"
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

BENCHMARK(BM_Parser)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<2>)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<4>)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<8>)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<16>)->Unit(benchmark::kSecond)->MinTime(10);
BENCHMARK(BM_Parser_Sorter<32>)->Unit(benchmark::kSecond)->MinTime(10);

BENCHMARK_MAIN();
