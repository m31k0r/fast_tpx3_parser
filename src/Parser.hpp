#pragma once
#include <sys/types.h>
#include <assert.hpp>
#include <chrono>
#include <cstdint>
#include <print>
#include <ranges>
#include <stop_token>
#include <thread>
#include <utility>
#include "Batch.hpp"
#include "Batch_Pool.hpp"
#include "Compare.hpp"
#include "Constants.hpp"
#include "Extract_Queue.hpp"
#include "MPMCQueue.h"
#include "Memory_Map.hpp"
#include "SPSCQueue.h"
#include "Sink.hpp"
#include "Sorter.hpp"
#include "tpx3_helper.hpp"

using U64MPMC = rigtorp::MPMCQueue<U64BatchPtr>;
using U64SPSC = rigtorp::SPSCQueue<U64BatchPtr>;

using TriggerMPMC = rigtorp::MPMCQueue<TriggerBatchPtr>;
using TriggerSPSC = rigtorp::SPSCQueue<TriggerBatchPtr>;

using U64InjSP = Injector<U64SPSC, U64BatchPtr, Batch_Pool<U64Batch>, uint64_t>;
using U64InjMP = Injector<U64MPMC, U64BatchPtr, Batch_Pool<U64Batch>, uint64_t>;

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
struct Pusher {
    std::array<U64InjSP, 4> hit_inj;
    U64InjMP trigger_inj;

    void insert_hit(uint64_t data, uint8_t t_chip_nr) {
        DEBUG_ASSERT(t_chip_nr < 4);
        while (not hit_inj[t_chip_nr].try_push(data)) {
        }
    }
    void insert_trigger(uint64_t data) {
        while (not trigger_inj.try_push(data)) {
        }
    };

    Pusher(std::array<U64SPSC, 4>& hit_queue, U64MPMC& trigger_queue,
           Batch_Pool<U64Batch>& batch_pool)
        : hit_inj(make_array_l<U64InjSP>(std::make_index_sequence<4>{},
                                         [&batch_pool, &hit_queue](int i) {
                                             return U64InjSP(hit_queue[i],
                                                             batch_pool);
                                         })),
          trigger_inj(trigger_queue, batch_pool) {}
};

template <size_t Trigger_Parse_Threads>
struct BatchHandle {
   public:
    Batch_Pool<U64Batch> U64batch_pool{BATCH_POOL_SIZE};
    Batch_Pool<TriggerBatch> Triggerbatch_pool{BATCH_POOL_SIZE};

    // Hits
    std::array<U64SPSC, 4> hit_queue =
        make_array<U64SPSC>(std::make_index_sequence<4>{}, QUEUE_SIZE);
    std::array<U64SPSC, 4> sorted_hit_queue =
        make_array<U64SPSC>(std::make_index_sequence<4>{}, QUEUE_SIZE);

    // Triggers
    U64MPMC trigger_queue{QUEUE_SIZE};
    std::array<TriggerSPSC, Trigger_Parse_Threads> parsed_triggers =
        make_array<TriggerSPSC>(
            std::make_index_sequence<Trigger_Parse_Threads>{}, QUEUE_SIZE);

    Pusher pusher;

   public:
    BatchHandle() : pusher(hit_queue, trigger_queue, U64batch_pool) {}

    BatchHandle(const BatchHandle&) = delete;
    BatchHandle& operator=(const BatchHandle&) = delete;
};

#include "Parse.hpp"

template <class Work, class InQueue, class OutQueue>
class Worker {
    std::jthread worker;
    InQueue& in;
    OutQueue& out;
    Work work;

   public:
    Worker(Work t_work, InQueue& t_in, OutQueue& t_out)
        : in(t_in), out(t_out), work(std::move(t_work)) {
        worker = std::jthread([this](std::stop_token stop_token) {
            pthread_setname_np(pthread_self(), "Worker");
            uint64_t seq_before [[maybe_unused]] = 0;
            while (not stop_token.stop_requested()) {
                U64BatchPtr ba;
                if (in.try_pop(ba)) {
                    const auto old_seq [[maybe_unused]] = ba->sequence_number;
                    auto res = work.parse(std::move(ba));
                    DEBUG_ASSERT(res);
                    DEBUG_ASSERT(res->sequence_number == old_seq);
                    DEBUG_ASSERT(old_seq == 0 || seq_before + 1 == old_seq);
                    seq_before = old_seq;
                    while (not out.try_push(std::move(res))) {
                    }
                }
            }
        });
    }
};
template <class InQueue, size_t N, typename T>
class SPSCs2SPSC {
    T current_batch;
    std::array<InQueue, N>& in;
    size_t sequence_number = 0;

    TriggerBatchPtr next_batch() {
        bool all_non_empty = true;
        for (auto& queue : in) {
            auto front = queue.front();
            if (not front) {
                all_non_empty = false;
                continue;
            }

            const auto front_seq_nr = front->get()->sequence_number;
            if (front_seq_nr < sequence_number) {
                PANIC("There should not be smaller seq number");
            };
            if (front_seq_nr == sequence_number) {
                TriggerBatchPtr batch = std::move(*front);
                queue.pop();
                sequence_number++;
                return batch;
            }
        }
        if (all_non_empty) {
            print_state();
            PANIC("All queues are full but no match");
        }
        return nullptr;
    }

   public:
    SPSCs2SPSC(std::array<InQueue, N>& queues) : in(queues) {}

    TriggerBatchPtr* front() {
        if (not current_batch) {
            current_batch = next_batch();
        }
        if (not current_batch) {
            return nullptr;
        }
        return &current_batch;
    };
    void pop() {
        DEBUG_ASSERT(current_batch,
                     "Should not pop if current_batch is nullptr");
        current_batch = nullptr;
    };

    void print_state() {
        std::print("Seq: {}  -  ", sequence_number);
        for (auto& queue : in) {
            auto front = queue.front();
            if (not front) {
                std::print("Empty; ");
                continue;
            }
            std::print("S:{}L:{}", front->get()->sequence_number, queue.size());
        }
        std::println();
    };
};

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

    // Trigger
    using Worker_t = Worker<ParseTrigger, U64MPMC, TriggerSPSC>;
    std::array<Worker_t, Trigger_Parse_Threads> m_parse_trigger =
        make_array_l<Worker_t>(
            std::make_index_sequence<Trigger_Parse_Threads>{}, [this](int i) {
                return Worker_t{m_batch_handle.Triggerbatch_pool,
                                m_batch_handle.trigger_queue,
                                m_batch_handle.parsed_triggers[i]};
            });

    using Merger =
        SPSCs2SPSC<TriggerSPSC, Trigger_Parse_Threads, TriggerBatchPtr>;
    Merger trigger_out_queue{m_batch_handle.parsed_triggers};

    // Hits
    using Sort_t = Sort<U64SPSC, U64SPSC, Compare_Hits>;
    std::array<Sort_t, 4> sort_hits =
        make_array_l<Sort_t>(std::make_index_sequence<4>{}, [this](int i) {
            return Sort_t(m_batch_handle.hit_queue[i],
                          m_batch_handle.sorted_hit_queue[i]);
        });

    // Temporary Sinks;
    std::array<Sink<U64SPSC>, 4> hit_sinks;
    Sink<Merger> trigger_sink;

   public:
    Parser(MemoryMap t_map)
        : header(m_batch_handle.pusher), map(std::move(t_map)) {
        for (int i = 0; i < 4; ++i) {
            sort_hits[i].start_worker();
            hit_sinks[i].start(m_batch_handle.sorted_hit_queue[i]);
        }
        trigger_sink.start(trigger_out_queue);
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
