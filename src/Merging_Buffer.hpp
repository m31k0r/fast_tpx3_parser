#pragma once
#include <sys/types.h>
#include <assert.hpp>
#include <cstdint>
#include "Batch.hpp"
#include "Compare.hpp"
#include "Constants.hpp"
#include "Parser.hpp"
#include "QueueConcepts.hpp"
#include "SPSCQueue.h"

#include <iostream>
#include <print>
#include <ratio>
#include <thread>

namespace detail {
struct Comp {
    size_t check;
    bool operator()(U64BatchPtr *batch) const {
        bool is_valid = batch->get()->sequence_number == check;
        if (not is_valid)
            std::println("Looking for seq nr: {} but found {}", check,
                         batch->get()->sequence_number);
        return is_valid;
    };
};
}  // namespace detail

template <typename InQueue, typename COMPARE, size_t NUMBER_OF_QUEUES = 2,
          size_t SORT_LENGTH = 2>
    requires SCQueueable<InQueue, U64BatchPtr>
class Merger {
    std::array<InQueue *, NUMBER_OF_QUEUES> m_queue;
    size_t m_sequence_number = 0;
    std::array<uint64_t, BATCH_SIZE * 2> buffer_;
    bool buffer_filled = false;

    U64BatchPtr current_batch = nullptr;
    U64BatchPtr next_batch() {
        // const detail::Comp comp(m_sequence_number);
        // bool all_non_empty = true;
        for (auto &queue : m_queue) {
            auto front = queue->front();
            if (not front) {
                // all_non_empty = false;
                continue;
            }

            const auto front_seq_nr = front->get()->sequence_number;
            if (front_seq_nr == m_sequence_number) {
                U64BatchPtr batch = std::move(*front);
                queue->pop();
                m_sequence_number++;
                return batch;
            }
        }
        // if (all_non_empty) {
        //     std::cout << "Error: All queues are non-empty but no batch with "
        //                  "sequence number "
        //               << m_sequence_number << " found." << std::endl;
        // }
        return nullptr;
    }

    void sort_head_tail(U64Batch *new_batch) {
        DEBUG_ASSERT(buffer_[0]);
        std::merge(new_batch->begin(), new_batch->end(),
                   buffer_.begin() + BATCH_SIZE, buffer_.end(),
                   buffer_.begin());
        std::move(buffer_.begin(), buffer_.begin() + BATCH_SIZE,
                  new_batch->begin());
    }
    U64BatchPtr get_batch() {
        auto new_batch = next_batch();
        if (new_batch == nullptr) {
            return nullptr;
        }
        if (not buffer_filled) {
            std::copy(new_batch->data.begin(), new_batch->data.end(),
                      buffer_.begin() + BATCH_SIZE);
            buffer_filled = true;
            new_batch = next_batch();
            if (new_batch == nullptr) {
                return nullptr;
            }
        }
        sort_head_tail(new_batch.get());
        new_batch->sequence_number--;
        return new_batch;
    }

   public:
    Merger(const std::array<InQueue *, NUMBER_OF_QUEUES> &queues)
        : m_queue(queues) {}

    U64Batch *front() {
        if (not current_batch) {
            current_batch = get_batch();
        }
        return current_batch.get();
    };
    void pop() {
        DEBUG_ASSERT(current_batch);
        current_batch = nullptr;
    };

    void print_state() {
        std::print("Seq: {}  -  ", m_sequence_number);
        for (auto &queue : m_queue) {
            auto front = queue->front();
            if (not front) {
                std::print("Empty; ");
                continue;
            }
            std::print("S:{}L:{}", front->get()->sequence_number,
                       queue->size());
        }
        std::println();
    };
};
