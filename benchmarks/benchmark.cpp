#include <benchmark/benchmark.h>
#include <filesystem>
#include <memory>
#include <thread>
#include "Compare.hpp"
#include "Constants.hpp"
#include "Memory_Map.hpp"
#include "Merging_Buffer.hpp"
#include "Parser.hpp"
#include "Queue_Status.hpp"
#include "Sink.hpp"
#include "Sorter.hpp"

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

static void BM_Parser_Sorter(benchmark::State &state) {
    Parser parser;
    ASSERT(std::filesystem::exists(TESTPATH), TESTPATH);
    MemoryMap map(TESTPATH);

    std::array<Sink<MPMC>, 4> hit_consumers;
    for (size_t i = 0; i < hit_consumers.size(); ++i) {
        hit_consumers[i].start(parser.get_batch_handle().m_hit_queue[i]);
    }

    constexpr int NUM_THREADS = 1;

    std::array<std::unique_ptr<SPSC>, NUM_THREADS> sorted_trigger_queue{};

    using SortT = Sort<MPMC, SPSC, Compare_Triggers>;
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

template <size_t NUM_THREADS>
static void BM_Parser_S_Merger(benchmark::State &state) {
    Parser parser;
    ASSERT(std::filesystem::exists(TESTPATH), TESTPATH);
    MemoryMap map(TESTPATH);

    std::array<Sink<MPMC>, 4> hit_consumers;
    for (size_t i = 0; i < hit_consumers.size(); ++i) {
        hit_consumers[i].start(parser.get_batch_handle().m_hit_queue[i]);
    }

    std::array<std::unique_ptr<SPSC>, NUM_THREADS> sorted_trigger_queue{};

    using SortT = Sort<MPMC, SPSC, Compare_Triggers>;
    std::array<std::unique_ptr<SortT>, NUM_THREADS> trigger_sorter;
    for (size_t i = 0; i < NUM_THREADS; ++i) {
        sorted_trigger_queue[i] = std::make_unique<SPSC>(QUEUE_SIZE);
        trigger_sorter[i] = std::make_unique<SortT>(
            parser.get_batch_handle().m_trigger_queue, *sorted_trigger_queue[i],
            parser.get_batch_handle().batch_pool);
        trigger_sorter[i]->start_worker();
    }

    std::array<SPSC *, NUM_THREADS> ptrs;
    for (size_t i = 0; i < NUM_THREADS; ++i) {
        ptrs[i] = sorted_trigger_queue[i].get();
    }
    Merger<SPSC, Compare_Triggers, NUM_THREADS> merger(ptrs);

    std::jthread sink([&merger](std::stop_token st) {
        pthread_setname_np(pthread_self(), "Merger");
        while (not st.stop_requested()) {
            if (merger.front()) {
                merger.pop();
            }
        }
    });

    for (auto _ : state) {
        parser.parse(map);
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            (map.end() - map.begin()) * 8);
}

static void BM_All() {
    Parser parser;
    ASSERT(std::filesystem::exists(TESTPATH), TESTPATH);
    MemoryMap map(TESTPATH);

    // TODO hit path
    std::array<Sink<MPMC>, 4> hit_consumers;
    for (size_t i = 0; i < hit_consumers.size(); ++i) {
        hit_consumers[i].start(parser.get_batch_handle().m_hit_queue[i]);
    }

    for (auto _ : state) {
        parser.parse(map);
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            (map.end() - map.begin()) * 8);
}

constexpr double MinTime = 1;

BENCHMARK(BM_Parser)->Unit(benchmark::kSecond)->MinTime(MinTime);
BENCHMARK(BM_Parser_Sorter)->Unit(benchmark::kSecond)->MinTime(MinTime);
BENCHMARK(BM_Parser_S_Merger<2>)->Unit(benchmark::kSecond)->MinTime(MinTime);
BENCHMARK(BM_Parser_S_Merger<4>)->Unit(benchmark::kSecond)->MinTime(MinTime);
BENCHMARK(BM_Parser_S_Merger<8>)->Unit(benchmark::kSecond)->MinTime(MinTime);
BENCHMARK(BM_Parser_S_Merger<16>)->Unit(benchmark::kSecond)->MinTime(MinTime);

BENCHMARK_MAIN();
