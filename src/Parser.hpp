#pragma once
#include <sys/types.h>
#include <assert.hpp>
#include <chrono>
#include <cstdint>
#include <ranges>
#include <utility>
#include "Batch.hpp"
#include "Batch_Pool.hpp"
#include "Constants.hpp"
#include "Extract_Queue.hpp"
#include "MPMCQueue.h"
#include "Memory_Map.hpp"
#include "SPSCQueue.h"
#include "Sink.hpp"
#include "tpx3_helper.hpp"

using U64MPMC = rigtorp::MPMCQueue<U64BatchPtr>;
using U64SPSC = rigtorp::SPSCQueue<U64BatchPtr>;

using TriggerMPMC = rigtorp::MPMCQueue<TriggerBatchPtr>;
using TriggerSPSC = rigtorp::SPSCQueue<TriggerBatchPtr>;

using U64Inj = Injector<U64MPMC, U64BatchPtr, Batch_Pool<U64Batch>, uint64_t>;

template <typename Element_t, typename... Args, std::size_t... I>
auto make_array(std::index_sequence<I...>, Args&&... args) {
    return std::array<Element_t, sizeof...(I)>{
        (static_cast<void>(I), Element_t{std::forward<Args>(args)...})...};
}
template <typename Element_t, typename F, std::size_t... I>
auto make_array_l(std::index_sequence<I...>, F&& f) {
    return std::array<Element_t, sizeof...(I)>{
        f(std::integral_constant<std::size_t, I>{})...};
}

template <size_t Trigger_Parse_Threads>
struct BatchHandle {
   public:
    Batch_Pool<U64Batch> U64batch_pool{BATCH_POOL_SIZE};
    Batch_Pool<TriggerBatch> Triggerbatch_pool{BATCH_POOL_SIZE};

    // Hits
    std::array<U64MPMC, 4> hit_queue =
        make_array<U64MPMC>(std::make_index_sequence<4>{}, QUEUE_SIZE);
    std::array<U64Inj, 4> hit_inj = make_array_l<U64Inj>(
        std::make_index_sequence<4>{},
        [this](int i) { return U64Inj{hit_queue[i], U64batch_pool}; });

    // Triggers
    U64MPMC trigger_queue{QUEUE_SIZE};
    std::array<TriggerSPSC, Trigger_Parse_Threads> parsed_triggers =
        make_array<TriggerSPSC>(
            std::make_index_sequence<Trigger_Parse_Threads>{}, QUEUE_SIZE);
    U64Inj trigger_inj{trigger_queue, U64batch_pool};
    TriggerSPSC out_trigger{QUEUE_SIZE};

   public:
    BatchHandle() {}

    void insert_hit(uint64_t data, uint8_t t_chip_nr) {
        DEBUG_ASSERT(t_chip_nr < 4);
        while (not hit_inj[t_chip_nr].try_push(data)) {
        }
    }
    void insert_trigger(uint64_t data) {};
    BatchHandle(const BatchHandle&) = delete;
    BatchHandle& operator=(const BatchHandle&) = delete;
};

#include "Parse.hpp"
template <size_t Trigger_Parse_Threads>
class Parser {
    BatchHandle<Trigger_Parse_Threads> m_batch_handle;
    std::array<ParseTrigger, Trigger_Parse_Threads> m_parser =
        make_array<ParseTrigger>(
            std::make_index_sequence<Trigger_Parse_Threads>{},
            m_batch_handle.Triggerbatch_pool);

    // Main Parser
    ParseHeader header;
    MemoryMap map;

    // Temporary Sinks;
    std::array<Sink<U64MPMC>, 4> hit_sinks;
    Sink<U64MPMC> trigger_sink;

   public:
    Parser(MemoryMap t_map) : header(m_batch_handle), map(std::move(t_map)) {
        for (int i = 0; i < 4; ++i) {
            hit_sinks[i].start(m_batch_handle.hit_queue[i]);
        }
        trigger_sink.start(m_batch_handle.trigger_queue);
    };
    void parse() {
        const auto start = std::chrono::steady_clock::now();
        auto ec [[maybe_unused]] = header.parse(map);
        const auto stop = std::chrono::steady_clock::now();
        const auto file_size_gb =
            std::distance(map.begin(), map.end()) * sizeof(uint64_t) * 1e-9;
        const auto time_s = std::chrono::duration<double>(stop - start).count();

        std::cout << "Filesize: " << file_size_gb << " Gb" << std::endl;
        std::cout << "Time: " << time_s << " s" << std::endl;
        std::cout << "Throughput: " << file_size_gb / time_s * num_runs
                  << " Gb/s" << std::endl;
    }
};
