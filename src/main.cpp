#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <iostream>
#include <iterator>
#include <thread>

#include "Batch.hpp"
#include "Batch_Pool.hpp"
#include "Memory_Map.hpp"
#include "Parser.hpp"
#include "Sink.hpp"
#include "Sorting_Buffer.hpp"
#include "mpmc_queue.h"
#include "tpx3_helper.hpp"

template <typename Compare, size_t WINDOW_SIZE = 64>
class Sort {
    std::jthread m_thread;
    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &m_hit_queue;
    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &m_sorted_hit_queue;
    SortingBuffer<uint64_t, WINDOW_SIZE, Compare> m_buffer;

    Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> &m_batch_pool;
    Batch<uint64_t> *m_current_batch;

   public:
    bool idle;

    Sort(es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &hit_queue,
         es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE>
             &sorted_hit_queue,
         Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> &batch_pool)
        : m_hit_queue(hit_queue),
          m_sorted_hit_queue(sorted_hit_queue),
          m_batch_pool(batch_pool) {
        m_current_batch = m_batch_pool.acquire_batch_blocking();
    }
    void start_worker() {
        m_thread = std::jthread([this](std::stop_token stop) {
            Batch<uint64_t> *batch;
            while (not stop.stop_requested()) {
                if (m_hit_queue.pop(batch)) {
                    idle = false;
                    for (size_t i = 0; i < batch->insert_idx; ++i) {
                        auto data = batch->data[i];
                        auto sorted_data = m_buffer.pushpop(data);
                        if (sorted_data.has_value()) {
                            exchange_batch_on_full(m_current_batch,
                                                   m_batch_pool,
                                                   m_sorted_hit_queue);
                            m_current_batch
                                ->data[m_current_batch->insert_idx++] =
                                sorted_data.value();
                        }
                    }
                    m_batch_pool.release_batch_blocking(batch);
                } else {
                    idle = true;
                }
            }
        });
    }
    void stop() { m_thread.request_stop(); }
};

int main_all() {
    Parser parser;

    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> sorted_hit_queue[4];
    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE>
        sorted_trigger_queue;
    std::array<std::unique_ptr<Sort<Compare_Hits, 8>>, 8> sorters;
    for (int chip = 0; chip < 4; ++chip) {
        sorters[chip] = std::make_unique<Sort<Compare_Hits, 8>>(
            parser.get_batch_handle().m_hit_queue[chip], sorted_hit_queue[chip],
            parser.get_batch_handle().batch_pool);
        sorters[chip]->start_worker();
    }
    Sort<Compare_Triggers, 8> trigger_sorter(
        parser.get_batch_handle().m_trigger_queue, sorted_trigger_queue,
        parser.get_batch_handle().batch_pool);
    trigger_sorter.start_worker();

    std::array<Hit_Consumer, 4> hit_consumers;
    for (size_t i = 0; i < hit_consumers.size(); ++i) {
        hit_consumers[i].start(sorted_hit_queue[i],
                               parser.get_batch_handle().batch_pool);
    }
    std::array<Trigger_Consumer, 1> trigger_consumers;
    for (size_t i = 0; i < trigger_consumers.size(); ++i) {
        trigger_consumers[i].start(sorted_trigger_queue,
                                   parser.get_batch_handle().batch_pool);
    }

    std::future<void> future = std::async(std::launch::async, [&parser] {
        auto mmap = MemoryMap("./tpx3.tpx3");
        parser.parse(mmap);
    });

    while (not(future.wait_for(std::chrono::milliseconds(500)) ==
               std::future_status::ready)) {
    }

    for (auto &consumer : hit_consumers) {
        consumer.stop();
    }
    for (auto &consumer : trigger_consumers) {
        consumer.stop();
    }
    for (auto &sorter : sorters) {
        sorter->stop();
    }

    return 0;
}
