#pragma once
#include "Batch.hpp"
#include "mpmc_queue.h"

template <typename Compare, size_t NUMBER_OF_QUEUES = 2>
class Merger {
    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE>
        *m_queue[NUMBER_OF_QUEUES];
    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> *m_output_queue;

    Merger(
        es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE>
            *queues[NUMBER_OF_QUEUES],
        es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> *output_queue)
        : m_queue(queues), m_output_queue(output_queue) {}

    void step() {}
};
