#pragma once
#include <assert.hpp>
#include <chrono>
#include <cstdint>
#include <print>
#include <utility>
#include "Batch/Batch.hpp"
#include "Batch/Batch_Pool.hpp"
#include "Compare.hpp"
#include "Constants.hpp"
#include "Operations/Delay.hpp"
#include "Operations/MergeSPSC.hpp"
#include "Operations/Merging_Buffer.hpp"
#include "Operations/Sink.hpp"
#include "Operations/Sorter.hpp"
#include "Operations/Worker.hpp"
#include "Parsing/ParseHeader.hpp"
#include "Parsing/ParseTrigger.hpp"
#include "QueueUtility/Extractor.hpp"
#include "QueueUtility/Injector.hpp"
#include "Utility/MPMCQueue.h"
#include "Utility/Memory_Map.hpp"
#include "Utility/SPSCQueue.h"
#include "Utility/helper.hpp"

using U64MPMC = rigtorp::MPMCQueue<U64BatchPtr>;
using U64SPSC = rigtorp::SPSCQueue<QueueMessage<U64BatchPtr>>;

using TriggerMPMC = rigtorp::MPMCQueue<TriggerBatchPtr>;
using TriggerSPSC = rigtorp::SPSCQueue<QueueMessage<TriggerBatchPtr>>;

using HitMPMC = rigtorp::MPMCQueue<HitBatchPtr>;
using HitSPSC = rigtorp::SPSCQueue<QueueMessage<HitBatchPtr>>;

using U64InjSP = Injector<U64SPSC, U64BatchPtr, Batch_Pool<U64Batch>, uint64_t>;
template <size_t N>
using U64InjSPA = Injector<std::array<U64SPSC, N>, U64BatchPtr,
                           Batch_Pool<U64Batch>, uint64_t>;

template <std::size_t N>
struct Pusher {
    std::array<U64InjSP, 4> hit_inj;
    U64InjSPA<N> trigger_inj;
    size_t H_backoff = 0;
    size_t T_backoff = 0;
    size_t H_event = 0;
    size_t T_event = 0;

    void insert_hit(uint64_t data, uint8_t t_chip_nr) {
        DEBUG_ASSERT(t_chip_nr < 4);
        adaptive_backoff backoff;
        while (not hit_inj[t_chip_nr].try_push(data)) {
            backoff.pause();
            H_backoff++;
        }
        H_event++;
    }
    void insert_trigger(uint64_t data) {
        adaptive_backoff backoff;
        while (not trigger_inj.try_push(data)) {
            backoff.pause();
            T_backoff++;
        }
        T_event++;
    };
    ~Pusher() {
        std::cout << "Backoff: " << H_backoff << " Hit & " << T_backoff
                  << " Trigger\n";
        std::cout << "Events: " << H_event << " Hits " << T_event
                  << " Trigger\n";
    }

    Pusher(std::array<U64SPSC, 4>& hit_queue,
           std::array<U64SPSC, N>& trigger_queue,
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
    Batch_Pool<HitBatch> Hitbatch_pool{BATCH_POOL_SIZE};

    // Hits
    std::array<U64SPSC, 4> hit_queue =
        make_array<U64SPSC>(std::make_index_sequence<4>{}, QUEUE_SIZE);
    std::array<HitSPSC, 4> sorted_hit_queue =
        make_array<HitSPSC>(std::make_index_sequence<4>{}, QUEUE_SIZE);

    // Triggers
    std::array<U64SPSC, Trigger_Parse_Threads> trigger_queue =
        make_array<U64SPSC>(std::make_index_sequence<Trigger_Parse_Threads>{},
                            QUEUE_SIZE);
    std::array<TriggerSPSC, Trigger_Parse_Threads> parsed_triggers =
        make_array<TriggerSPSC>(
            std::make_index_sequence<Trigger_Parse_Threads>{}, QUEUE_SIZE);

    Pusher<Trigger_Parse_Threads> pusher;

   public:
    BatchHandle() : pusher(hit_queue, trigger_queue, U64batch_pool) {}

    BatchHandle(const BatchHandle&) = delete;
    BatchHandle& operator=(const BatchHandle&) = delete;
};

template <size_t Trigger_Parse_Threads>
class Parser {
    Camera camera_;
    BatchHandle<Trigger_Parse_Threads> m_batch_handle;
    std::array<ParseTrigger, Trigger_Parse_Threads> m_parser =
        make_array<ParseTrigger>(
            std::make_index_sequence<Trigger_Parse_Threads>{},
            m_batch_handle.Triggerbatch_pool);

    // Main Parser
    ParseHeader<Pusher<Trigger_Parse_Threads>> header;
    MemoryMap map;

    // Trigger
    using Worker_t = Worker<ParseTrigger, U64SPSC, TriggerSPSC>;
    std::array<Worker_t, Trigger_Parse_Threads> m_parse_trigger =
        make_array_l<Worker_t>(
            std::make_index_sequence<Trigger_Parse_Threads>{}, [this](int i) {
                return Worker_t{m_batch_handle.Triggerbatch_pool,
                                m_batch_handle.trigger_queue[i],
                                m_batch_handle.parsed_triggers[i]};
            });

    using TriggerMerger =
        SPSCs2SPSC<TriggerSPSC, Trigger_Parse_Threads, TriggerBatchPtr>;
    TriggerMerger trigger_out_queue{m_batch_handle.parsed_triggers};

    // Hits
    using Sort_t = Sort<U64SPSC, HitSPSC>;
    std::array<Sort_t, 4> sort_hits =
        make_array_l<Sort_t>(std::make_index_sequence<4>{}, [this](int i) {
            return Sort_t(m_batch_handle.hit_queue[i],
                          m_batch_handle.sorted_hit_queue[i],
                          m_batch_handle.Hitbatch_pool, camera_,
                          static_cast<uint8_t>(i));
        });

    // std::array<Sink<HitSPSC, HitBatchPtr>, 4> sinks;
    // std::array<Sink<U64SPSC, U64BatchPtr>, 4> sinks;

    Merger merger{m_batch_handle.Hitbatch_pool,
                  make_array_l<Extractor<HitSPSC, HitBatchPtr>>(
                      std::make_index_sequence<4>{}, [this](size_t i) {
                          return Extractor<HitSPSC, HitBatchPtr>(
                              m_batch_handle.sorted_hit_queue[i]);
                      })};

    // Temporary Sinks;
    // Sink<Merger, HitBatchPtr> hit_sink;
    // Sink<TriggerMerger, TriggerBatchPtr> trigger_sink;
    //
    //
    HitSPSC out_queue{QUEUE_SIZE};
    Delay<Merger, TriggerMerger, HitSPSC> delay;
    Sink<HitSPSC, HitBatchPtr> sink;

   public:
    Parser(MemoryMap t_map)
        : header(m_batch_handle.pusher),
          map(std::move(t_map)),
          delay(merger, trigger_out_queue, out_queue,
                m_batch_handle.Hitbatch_pool) {
        for (int i = 0; i < 4; ++i) {
            sort_hits[i].start_worker();
            // sinks[i].start(m_batch_handle.sorted_hit_queue[i]);
            // sinks[i].start(m_batch_handle.hit_queue[i]);
        }
        sink.start(out_queue);
        // o
        // hit_sink.start(merger);
        // merger.set_stop_token(hit_sink.get_stop_token());
        // trigger_sink.start(trigger_out_queue);
    };
    void parse() { auto ec [[maybe_unused]] = header.parse(map); }
    void flush() {
        std::ranges::for_each(m_batch_handle.pusher.hit_inj,
                              [](auto& inj) { inj.flush(); });
        m_batch_handle.pusher.trigger_inj.flush();
    }
    void finish() {
        std::ranges::for_each(m_batch_handle.pusher.hit_inj,
                              [](auto& inj) { inj.finish(); });
        m_batch_handle.pusher.trigger_inj.finish();
        sink.wait();
    }
};
