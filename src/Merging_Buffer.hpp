#pragma once
#include <cstdint>
#include "Batch.hpp"
#include "mpmc_queue.h"

namespace detail {
using BA = Batch<uint64_t>;
struct Comp {
    int check;
    bool operator()(BA *batch, auto a) const {
        return batch->sequence_number == check;
    };
};
}  // namespace detail

template <typename Compare, size_t NUMBER_OF_QUEUES = 2>
class Merger {
    using BA = Batch<uint64_t>;
    std::array<es::lockfree::mpmc_queue<BA *, QUEUE_SIZE> *, NUMBER_OF_QUEUES>
        m_queue;
    es::lockfree::mpmc_queue<BA *, QUEUE_SIZE> *m_output_queue;
    BA *last_batch = nullptr;
    size_t m_sequence_number = 0;

    BA *next_batch() {
        const detail::Comp comp(m_sequence_number++);
        bool all_non_empty = true;
        for (auto &queue : m_queue) {
            if (queue->empty()) {
                all_non_empty = false;
            }
            BA *batch;
            auto valid_pop = queue->pop_if(comp, batch);
            if (valid_pop) return batch;
        }
    }

   public:
    Merger(const std::array<es::lockfree::mpmc_queue<BA *, QUEUE_SIZE> *,
                            NUMBER_OF_QUEUES> &queues,
           es::lockfree::mpmc_queue<BA *, QUEUE_SIZE> *output_queue)
        : m_output_queue(output_queue) {
        m_queue = queues;
    }

    void startup() { last_batch = next_batch(); }
    void sort_head_tail(BA *old, BA *new) {}
    void step() {
        BA *new_batch = next_batch();
        sort_head_tail(last_batch, new_batch);
        // get new batch with correct sequence number;
        // sort the last elements betweeen them;
        // Push to queue;
    }
};
