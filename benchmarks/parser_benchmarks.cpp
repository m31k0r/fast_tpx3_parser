// Per-operation / per-thread benchmarks for the components wired together in
// Parser.hpp (the code compiled into Parser.cpp).
//
// Stage                      Thread in Parser          Benchmarks
// -------------------------  ------------------------  ----------------------
// ParseHeader (chunk walk)   caller of Parser::parse   Header_*
// Pusher / Injector          caller of Parser::parse   Injector_*, Header_Dispatch
// Worker<ParseTrigger>       "Worker" threads          ParseTrigger_*, Worker_*
// Sort (sort+merge+ParseHit) "Sorter" threads          Sort_*, ParseHit_*
// Merger                     "Sink" (hit) thread       Merger_FillBatch
// SPSCs2SPSC                 "Sink" (trigger) thread   TriggerMerger_*
// Sink                       "Sink" threads            Sink_Drain
//
// Stages are measured single-threaded so each number is the cost of one
// operation. Worker_* additionally measures the real worker thread. The Sort
// thread is not benchmarked end to end because it never emits output (see
// Sorter.hpp: `buffer_filled` is never set); its sort and merge steps are
// benchmarked instead.
#include <benchmark/benchmark.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <vector>

#include "Parser.hpp"

namespace {

constexpr size_t kChunkWords = 8000;  // chunk size field is 16 bits of bytes

// Hit time spans stay well below half of the 14-bit ToA range so that
// Compare_Hits is a valid ordering for the generated data.
std::vector<uint64_t> make_hit_words(size_t n, uint64_t seed = 1,
                                     uint64_t start_time = 0) {
    std::mt19937_64 rng(seed);
    std::vector<uint64_t> words;
    words.reserve(n);
    uint64_t t = start_time;
    for (size_t i = 0; i < n; ++i) {
        t += rng() % 4;
        const uint64_t ts = t + rng() % 32;  // local disorder
        const uint64_t toa = ts & 0x3FFF;
        const uint64_t spidr = (ts >> 14) & 0xFFFF;
        const uint64_t pix = rng() & 0xFFFF;
        const uint64_t tot = rng() & 0x3FF;
        const uint64_t ftoa = rng() & 0xF;
        words.push_back((HIT_NIBBLE << 60) | (pix << 44) | (toa << 30) |
                        (tot << 20) | (ftoa << 16) | spidr);
    }
    return words;
}

std::vector<uint64_t> make_trigger_words(size_t n, uint64_t seed = 2) {
    std::mt19937_64 rng(seed);
    std::vector<uint64_t> words;
    words.reserve(n);
    uint64_t ts = 0;
    for (size_t i = 0; i < n; ++i) {
        ts += 1 + rng() % 100;
        const uint64_t stamp = 1 + rng() % 12;
        const uint64_t type = 0xF;
        const uint64_t counter = i & 0xFFF;
        words.push_back((TRIGGER_NIBBLE << 60) | (type << 56) |
                        (counter << 44) | ((ts & ((1ULL << 35) - 1)) << 9) |
                        (stamp << 5));
    }
    return words;
}

uint64_t make_header_word(uint8_t chip, size_t payload_words) {
    const uint64_t bytes = payload_words * sizeof(uint64_t);
    return 0x33585054ULL | (uint64_t(chip) << 32) | (bytes << 48);
}

// Batches keep their contents when returned to the pool, so filling every
// pooled batch once lets benchmarks acquire "full" batches at no copy cost.
void prefill(Batch_Pool<U64Batch>& pool, const std::vector<uint64_t>& words) {
    for (auto& b : pool.m_batches) {
        std::copy_n(words.begin(), std::min(words.size(), BATCH_SIZE),
                    b->data.begin());
    }
}

U64BatchPtr acquire_full(Batch_Pool<U64Batch>& pool, size_t size) {
    auto batch = pool.acquire_batch_blocking();
    batch->size = size;
    return batch;
}

template <class Handle>
void drain(Handle& h) {
    for (auto& q : h.hit_queue) {
        while (q.front()) q.pop();
    }
    for (auto& q : h.trigger_queue) {
        while (q.front()) q.pop();
    }
}

// ---------------------------------------------------------------------------
// ParseHeader: walking chunks
// ---------------------------------------------------------------------------

void BM_Header_ParseChunkHeader(benchmark::State& state) {
    const size_t payload = static_cast<size_t>(state.range(0));
    constexpr size_t chunks = 256;
    std::vector<uint64_t> buf;
    for (size_t c = 0; c < chunks; ++c) {
        buf.push_back(make_header_word(static_cast<uint8_t>(c % 4), payload));
        buf.insert(buf.end(), payload, HIT_NIBBLE << 60);
    }
    for (auto _ : state) {
        const uint64_t* p = buf.data();
        const uint64_t* end = buf.data() + buf.size();
        while (p < end) {
            auto h = tpx3_helper::parse_header(p);
            benchmark::DoNotOptimize(h);
            p = h->end;
        }
    }
    state.SetItemsProcessed(state.iterations() * chunks);
}
BENCHMARK(BM_Header_ParseChunkHeader)->Name("Header/ParseChunkHeader")->Arg(8)->Arg(1000);

// Classify every word in a chunk and push it through the real Pusher. Queue
// draining (the consumer side) is included in the timed region.
void BM_Header_Dispatch(benchmark::State& state) {
    const auto trigger_pct = static_cast<size_t>(state.range(0));
    auto hits = make_hit_words(kChunkWords);
    auto triggers = make_trigger_words(kChunkWords);
    std::vector<uint64_t> buf;
    buf.push_back(make_header_word(1, kChunkWords));
    std::mt19937_64 rng(3);
    for (size_t i = 0; i < kChunkWords; ++i) {
        buf.push_back(rng() % 100 < trigger_pct ? triggers[i] : hits[i]);
    }

    BatchHandle<2> handle;
    ParseHeader<Pusher<2>> header(handle.pusher);
    const tpx3_helper::Header h{buf.data(), buf.data() + buf.size(), 1};

    for (auto _ : state) {
        header.parse_header(h);
        drain(handle);
    }
    state.SetItemsProcessed(state.iterations() * kChunkWords);
    state.SetBytesProcessed(state.iterations() * kChunkWords * sizeof(uint64_t));
}
BENCHMARK(BM_Header_Dispatch)->Name("Header/Dispatch/TriggerPct")->Arg(0)->Arg(10)->Arg(50);

// ---------------------------------------------------------------------------
// Injector (batching of encoded words)
// ---------------------------------------------------------------------------

void BM_Injector_TryPush(benchmark::State& state) {
    constexpr size_t n = BATCH_SIZE * 8;
    Batch_Pool<U64Batch> pool{64};
    U64SPSC queue(QUEUE_SIZE);
    U64InjSP injector(queue, pool);
    for (auto _ : state) {
        for (uint64_t i = 0; i < n; ++i) {
            if (not injector.try_push(i)) state.SkipWithError("queue full");
        }
        while (queue.front()) queue.pop();
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Injector_TryPush)->Name("Injector/TryPush");

// ---------------------------------------------------------------------------
// Worker: ParseTrigger
// ---------------------------------------------------------------------------

void BM_ParseTrigger_Parse(benchmark::State& state) {
    const auto n = static_cast<size_t>(state.range(0));
    Batch_Pool<U64Batch> in_pool{8};
    Batch_Pool<TriggerBatch> out_pool{8};
    prefill(in_pool, make_trigger_words(BATCH_SIZE));
    auto in = acquire_full(in_pool, n);
    ParseTrigger parser(out_pool);
    for (auto _ : state) {
        auto out = parser.parse(in);
        benchmark::DoNotOptimize(out->toa.data());
    }
    state.SetItemsProcessed(state.iterations() * n);
}
// The clock-correction loop in ParseTrigger runs over the whole batch array,
// so a partially filled batch costs almost as much as a full one.
BENCHMARK(BM_ParseTrigger_Parse)
    ->Name("ParseTrigger/Parse/BatchFill")
    ->Arg(BATCH_SIZE)
    ->Arg(BATCH_SIZE / 4);

// ---------------------------------------------------------------------------
// Sort thread: parts of the loop body, plus ParseHit
// ---------------------------------------------------------------------------

void BM_ParseHit_Parse(benchmark::State& state) {
    Batch_Pool<U64Batch> in_pool{8};
    Batch_Pool<HitBatch> out_pool{8};
    prefill(in_pool, make_hit_words(BATCH_SIZE));
    ParseHit parser(out_pool);
    for (auto _ : state) {
        auto out = parser.parse(acquire_full(in_pool, BATCH_SIZE));
        benchmark::DoNotOptimize(out->toa.data());
    }
    state.SetItemsProcessed(state.iterations() * BATCH_SIZE);
}
BENCHMARK(BM_ParseHit_Parse)->Name("ParseHit/Parse");

void BM_Sort_CopyBaseline(benchmark::State& state) {
    const auto src = make_hit_words(BATCH_SIZE);
    std::vector<uint64_t> work(BATCH_SIZE);
    for (auto _ : state) {
        std::copy(src.begin(), src.end(), work.begin());
        benchmark::DoNotOptimize(work.data());
    }
    state.SetItemsProcessed(state.iterations() * BATCH_SIZE);
}
BENCHMARK(BM_Sort_CopyBaseline)->Name("Sort/CopyBaseline");

// std::sort of one incoming batch (includes the copy measured above).
void BM_Sort_StdSort(benchmark::State& state) {
    const auto src = make_hit_words(BATCH_SIZE);
    std::vector<uint64_t> work(BATCH_SIZE);
    for (auto _ : state) {
        std::copy(src.begin(), src.end(), work.begin());
        std::sort(work.begin(), work.end(), Compare_Hits());
        benchmark::DoNotOptimize(work.data());
    }
    state.SetItemsProcessed(state.iterations() * BATCH_SIZE);
}
BENCHMARK(BM_Sort_StdSort)->Name("Sort/StdSort");

// std::merge with the retained batch plus the move-back, as in Sorter.hpp
// (includes two batch copies to reset the inputs).
void BM_Sort_MergeAndMove(benchmark::State& state) {
    auto a = make_hit_words(BATCH_SIZE, 1, 0);
    auto b = make_hit_words(BATCH_SIZE, 2, BATCH_SIZE * 2);
    std::sort(a.begin(), a.end(), Compare_Hits());
    std::sort(b.begin(), b.end(), Compare_Hits());
    std::vector<uint64_t> batch(BATCH_SIZE);
    std::vector<uint64_t> buffer(BATCH_SIZE * 2);
    for (auto _ : state) {
        std::copy(a.begin(), a.end(), batch.begin());
        std::copy(b.begin(), b.end(), buffer.begin() + BATCH_SIZE);
        std::merge(batch.begin(), batch.end(), buffer.begin() + BATCH_SIZE,
                   buffer.end(), buffer.begin());
        std::move(buffer.begin(), buffer.begin() + BATCH_SIZE, batch.begin());
        benchmark::DoNotOptimize(batch.data());
    }
    state.SetItemsProcessed(state.iterations() * BATCH_SIZE);
}
BENCHMARK(BM_Sort_MergeAndMove)->Name("Sort/MergeAndMove");

// ---------------------------------------------------------------------------
// Merger (hit Sink thread): 4-way merge of sorted per-chip batches
// ---------------------------------------------------------------------------

void BM_Merger_FillBatch(benchmark::State& state) {
    using Ext = Extractor<HitSPSC, HitBatchPtr>;
    constexpr size_t per_chip = BATCH_SIZE / 4;

    Batch_Pool<HitBatch> pool{64};
    auto queues = make_array<HitSPSC>(std::make_index_sequence<4>{}, QUEUE_SIZE);
    Merger merger(pool, Merger::Array{Ext(queues[0]), Ext(queues[1]),
                                      Ext(queues[2]), Ext(queues[3])});

    // Every round holds exactly BATCH_SIZE hits, evenly spread over the chips.
    size_t round = 0;
    auto push_round = [&] {
        for (size_t chip = 0; chip < 4; ++chip) {
            auto b = pool.acquire_batch_blocking();
            for (size_t i = 0; i < per_chip; ++i) {
                b->push_back(round * BATCH_SIZE + 4 * i + chip, 1,
                             static_cast<uint16_t>(chip),
                             static_cast<uint16_t>(i));
            }
            if (not queues[chip].try_push(std::move(b))) {
                state.SkipWithError("queue full");
            }
        }
        ++round;
    };

    // The merger needs a lookahead element on every chip, so keep one round
    // pushed ahead of the one being merged.
    push_round();
    for (auto _ : state) {
        state.PauseTiming();
        push_round();
        state.ResumeTiming();
        benchmark::DoNotOptimize(merger.front());
        merger.pop();
    }
    state.SetItemsProcessed(state.iterations() * BATCH_SIZE);
}
BENCHMARK(BM_Merger_FillBatch)->Name("Merger/FillBatch");

// ---------------------------------------------------------------------------
// Trigger merger (SPSCs2SPSC) and Sink
// ---------------------------------------------------------------------------

void BM_TriggerMerger_Next(benchmark::State& state) {
    constexpr size_t N = 2;
    using Merger_t = SPSCs2SPSC<TriggerSPSC, N, TriggerBatchPtr>;

    Batch_Pool<TriggerBatch> pool{32};
    auto queues = make_array<TriggerSPSC>(std::make_index_sequence<N>{}, QUEUE_SIZE);
    Merger_t merger(queues);

    // Includes the (cheap) acquire + push that feeds the merger.
    for (auto _ : state) {
        auto b = pool.acquire_batch_blocking();
        const auto seq = b->sequence_number;
        if (not queues[seq % N].try_push(std::move(b))) {
            state.SkipWithError("queue full");
        }
        benchmark::DoNotOptimize(merger.front());
        merger.pop();
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TriggerMerger_Next)->Name("TriggerMerger/Next");

void BM_Sink_Drain(benchmark::State& state) {
    Batch_Pool<TriggerBatch> pool{32};
    TriggerSPSC queue(QUEUE_SIZE);
    Sink<TriggerSPSC, TriggerBatchPtr> sink;

    // Includes the acquire + push that feeds the sink.
    for (auto _ : state) {
        if (not queue.try_push(pool.acquire_batch_blocking())) {
            state.SkipWithError("queue full");
        }
        sink(queue);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Sink_Drain)->Name("Sink/Drain");

// ---------------------------------------------------------------------------
// Worker threads: throughput with a producer feeding `depth` batches in flight
// ---------------------------------------------------------------------------

template <class Work, class OutPool, class OutQueue>
void run_worker(benchmark::State& state, const std::vector<uint64_t>& words) {
    const auto depth = static_cast<size_t>(state.range(0));

    Batch_Pool<U64Batch> in_pool{64};
    OutPool out_pool{64};
    prefill(in_pool, words);
    U64SPSC in(QUEUE_SIZE);
    OutQueue out(QUEUE_SIZE);
    Worker<Work, U64SPSC, OutQueue> worker(Work(out_pool), in, out);

    size_t inflight = 0;
    auto drain_out = [&] {
        while (out.front()) {
            out.pop();
            --inflight;
        }
    };

    for (auto _ : state) {
        while (inflight >= depth) drain_out();
        auto b = acquire_full(in_pool, BATCH_SIZE);
        while (not in.try_push(std::move(b))) drain_out();
        ++inflight;
    }
    while (inflight > 0) drain_out();

    state.SetItemsProcessed(state.iterations() * BATCH_SIZE);
    state.SetBytesProcessed(state.iterations() * BATCH_SIZE * sizeof(uint64_t));
}

void BM_Worker_ParseTrigger(benchmark::State& state) {
    run_worker<ParseTrigger, Batch_Pool<TriggerBatch>, TriggerSPSC>(
        state, make_trigger_words(BATCH_SIZE));
}
BENCHMARK(BM_Worker_ParseTrigger)->Name("Worker/ParseTrigger/Depth")->Arg(1)->Arg(4)->Arg(8)->UseRealTime();

void BM_Worker_ParseHit(benchmark::State& state) {
    run_worker<ParseHit, Batch_Pool<HitBatch>, HitSPSC>(
        state, make_hit_words(BATCH_SIZE));
}
BENCHMARK(BM_Worker_ParseHit)->Name("Worker/ParseHit/Depth")->Arg(1)->Arg(4)->Arg(8)->UseRealTime();

}  // namespace

BENCHMARK_MAIN();
