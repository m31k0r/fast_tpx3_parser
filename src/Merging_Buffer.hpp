#pragma once
#include <cstdint>
#include "Batch.hpp"
#include "Compare.hpp"
#include "Constants.hpp"
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

template <typename COMPARE, size_t NUMBER_OF_QUEUES = 2, size_t SORT_LENGTH = 2>
class Merger {
    using BA = Batch<uint64_t>;
    std::array<es::lockfree::mpmc_queue<BA *, QUEUE_SIZE> *, NUMBER_OF_QUEUES>
        m_queue;
    es::lockfree::mpmc_queue<BA *, QUEUE_SIZE> *m_output_queue;
    BA *last_batch = nullptr;
    size_t m_sequence_number = 0;

   public:
    BA *next_batch() {
        const detail::Comp comp(m_sequence_number);
        bool all_non_empty = true;
        for (auto &queue : m_queue) {
            if (queue->empty()) {
                all_non_empty = false;
            }
            BA *batch;
            auto valid_pop = queue->pop_if(comp, batch);
            if (valid_pop) {
                m_sequence_number++;
                return batch;
            }
        }
        if (all_non_empty) {
            std::cout << "Error: All queues are non-empty but no batch with "
                         "sequence number "
                      << m_sequence_number << " found." << std::endl;
        }
        return nullptr;
    }

    Merger(const std::array<es::lockfree::mpmc_queue<BA *, QUEUE_SIZE> *,
                            NUMBER_OF_QUEUES> &queues,
           es::lockfree::mpmc_queue<BA *, QUEUE_SIZE> *output_queue)
        : m_output_queue(output_queue) {
        m_queue = queues;
    }

    void startup() {
        auto first_batch = next_batch();
        while (first_batch == nullptr) {
            first_batch = next_batch();
        }
        last_batch = first_batch;
    }
    void sort_head_tail(BA *old_batch, BA *new_batch) {
        uint64_t temp[SORT_LENGTH * 2];
        for (size_t i = 0; i < SORT_LENGTH; ++i) {
            temp[i] = old_batch->data[BATCH_SIZE - SORT_LENGTH + i];
            temp[SORT_LENGTH + i] = new_batch->data[i];
        }

        std::sort(std::begin(temp), std::end(temp), COMPARE{});

        for (size_t i = 0; i < SORT_LENGTH; ++i) {
            old_batch->data[BATCH_SIZE - SORT_LENGTH + i] = temp[i];
            new_batch->data[i] = temp[SORT_LENGTH + i];
        }
    }
    void step() {
        BA *new_batch = next_batch();
        if (new_batch == nullptr) {
            return;
        }
        sort_head_tail(last_batch, new_batch);
        while (not m_output_queue->push(last_batch)) {
            std::cout << "Output queue full, waiting to push batch with "
                         "sequence number "
                      << last_batch->sequence_number << std::endl;
        }
        last_batch = new_batch;
    }
};
