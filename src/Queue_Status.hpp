#pragma once

#include <chrono>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "Batch.hpp"
#include "MPMCQueue.h"
#include "SPSCQueue.h"

using MPMC = rigtorp::MPMCQueue<U64BatchPtr>;
using SPSC = rigtorp::SPSCQueue<U64BatchPtr>;

using QueueVariant = std::variant<MPMC*, SPSC*>;

class Queue_State_Printer {
    std::vector<QueueVariant> m_queues;
    std::vector<std::string> m_queue_names;
    std::chrono::milliseconds m_interval{1000};

    std::jthread m_thread;

    void run();

   public:
    Queue_State_Printer(
        std::vector<QueueVariant> queues,
        std::vector<std::string> queue_names = {},
        std::chrono::milliseconds interval = std::chrono::milliseconds{1000});
};
