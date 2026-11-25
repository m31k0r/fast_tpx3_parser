#include <benchmark/benchmark.h>

#include <vector>

#include "Parsing/ParseHeader.hpp"

namespace {

struct CountingPusher {
    size_t hits = 0;
    size_t triggers = 0;
    size_t flushes = 0;

    void insert_hit(uint64_t, uint8_t) { ++hits; }
    void insert_trigger(uint64_t) { ++triggers; }
    void flush() { ++flushes; }
};

}  // namespace

static void BM_parse_header_dispatch(benchmark::State& state) {
    constexpr size_t word_count = 4096;
    std::vector<uint64_t> words(word_count);
    for (size_t i = 0; i < word_count; ++i) {
        const auto nibble =
            i % 64 == 0 ? FRAME_NIBBLE
                        : (i % 2 == 0 ? HIT_NIBBLE : TRIGGER_NIBBLE);
        words[i] = nibble << 60;
    }

    CountingPusher pusher;
    ParseHeader<CountingPusher> parser(pusher);
    const tpx3_helper::Header header{
        words.data(), words.data() + words.size(), 0};

    for (auto _ : state) {
        parser.parse_header(header);
    }

    benchmark::DoNotOptimize(pusher.hits);
    benchmark::DoNotOptimize(pusher.triggers);
    benchmark::DoNotOptimize(pusher.flushes);
    state.SetItemsProcessed(state.iterations() * word_count);
}

BENCHMARK(BM_parse_header_dispatch);
