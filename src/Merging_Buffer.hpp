#pragma once
#include <cstdint>
#include "Batch.hpp"
#include "Compare.hpp"
#include "Constants.hpp"
#include "QueueConcepts.hpp"
#include "SPSCQueue.h"

#include <iostream>
#include <print>

namespace detail {
struct Comp {
    int check;
    bool operator()(U64BatchPtr *batch) const {
        bool is_valid = batch->get()->sequence_number == check;
        if (not is_valid)
            std::println("Looking for seq nr: {} but found {}", check,
                         batch->get()->sequence_number);
        return is_valid;
    };
};
}  // namespace detail

template <typename InQueue, typename OutQueue, typename COMPARE,
          size_t NUMBER_OF_QUEUES = 2, size_t SORT_LENGTH = 2>
    requires SCQueueable<InQueue, U64BatchPtr> &&
             InQueueable<OutQueue, U64BatchPtr>
class Merger {
    std::array<InQueue *, NUMBER_OF_QUEUES> m_queue;
    OutQueue &m_output_queue;
    U64BatchPtr last_batch = nullptr;
    size_t m_sequence_number = 0;

   public:
    U64BatchPtr next_batch() {
        const detail::Comp comp(m_sequence_number);
        bool all_non_empty = true;
        for (auto &queue : m_queue) {
            auto front = queue->front();
            if (not front) {
                all_non_empty = false;
                continue;
            }

            if (front->get()->sequence_number == m_sequence_number) {
                U64BatchPtr batch = std::move(*front);
                queue->pop();
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

    Merger(const std::array<InQueue *, NUMBER_OF_QUEUES> &queues,
           OutQueue &output_queue)
        : m_output_queue(output_queue), m_queue(queues) {}

    void startup() {
        auto first_batch = next_batch();
        while (first_batch == nullptr) {
            first_batch = next_batch();
        }
        last_batch = std::move(first_batch);
    }
    void sort_head_tail(Batch<uint64_t> *old_batch,
                        Batch<uint64_t> *new_batch) {
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
        auto new_batch = next_batch();
        if (new_batch == nullptr) {
            return;
        }
        sort_head_tail(last_batch.get(), new_batch.get());
        while (not m_output_queue->try_push(std::move(last_batch))) {
            std::cout << "Output queue full, waiting to push batch with "
                         "sequence number "
                      << last_batch->sequence_number << std::endl;
        }
        last_batch = std::move(new_batch);
    }
};
