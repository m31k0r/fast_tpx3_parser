#pragma once
#include <algorithm>
#include <libassert/assert.hpp>
#include <thread>

#include "Batch.hpp"
#include "Constants.hpp"
#include "QueueConcepts.hpp"
#include "Sorting_Buffer.hpp"

template <typename InQueue, typename OutQueue, class Compare>
    requires MCQueueable<InQueue, U64BatchPtr> &&
             SCQueueable<OutQueue, U64BatchPtr>
class Sort {
    InQueue &m_hit_queue;
    OutQueue &m_sorted_hit_queue;

    std::jthread m_thread;

   public:
    Sort(InQueue &input_queue, OutQueue &output_queue,
         Batch_Pool<U64Batch> &batch_pool)
        : m_hit_queue(input_queue), m_sorted_hit_queue(output_queue) {
        DEBUG_ASSERT(m_current_batch);
    }

    void start_worker() {
        m_thread = std::jthread([this](std::stop_token stop) {
            pthread_setname_np(pthread_self(), "Sorter");
            while (not stop.stop_requested()) {
                U64BatchPtr batch;
                if (m_hit_queue.try_pop(batch)) {
                    DEBUG_ASSERT(batch);

                    std::sort(batch->begin(), batch->end(), Compare());
                    // std::for_each(batch->begin(), batch->end(), Parse());

                    while (not m_sorted_hit_queue.try_push(std::move(batch))) {
                        if (stop.stop_requested()) {
                            return;
                        }
                    }
                }
            }
        });
    }
};
