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
