#pragma once
#include <chrono>
#include <thread>
#include "Batch.hpp"
#include "mpmc_queue.h"

class Queue_State_Printer {
    std::vector<es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> *>
        m_queues;
    std::vector<std::string> m_queue_names;

    std::jthread m_thread;
    std::chrono::milliseconds m_interval{1000};
    void run();

   public:
    Queue_State_Printer(
        const std::vector<
            es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> *> &queues,
        std::vector<std::string> queue_names = {},
        std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
};
