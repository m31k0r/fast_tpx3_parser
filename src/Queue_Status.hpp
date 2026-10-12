#pragma once
#include <chrono>
#include <thread>
#include "Batch.hpp"
#include "MPMCQueue.h"

class Queue_State_Printer {
    std::vector<rigtorp::MPMCQueue<U64BatchPtr> *> m_queues;
    std::vector<std::string> m_queue_names;

    std::jthread m_thread;
    std::chrono::milliseconds m_interval{1000};
    void run();

   public:
    Queue_State_Printer(
        const std::vector<rigtorp::MPMCQueue<U64BatchPtr> *> &queues,
        std::vector<std::string> queue_names = {},
        std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
};
