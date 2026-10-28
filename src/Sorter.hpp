#pragma once
#include <algorithm>
#include <libassert/assert.hpp>
#include <thread>

#include "Batch.hpp"
#include "Constants.hpp"
#include "Parse.hpp"
#include "QueueConcepts.hpp"
#include "Sorting_Buffer.hpp"

template <typename InQueue, typename OutQueue, class Compare>
    requires SCQueueable<InQueue, U64BatchPtr> &&
             SCQueueable<OutQueue, HitBatchPtr>
class Sort {
    InQueue &m_hit_queue;
    OutQueue &m_sorted_hit_queue;
    ParseHit parsehit;

    std::jthread m_thread;

    std::array<uint64_t, BATCH_SIZE * 2> buffer_;
    bool buffer_filled = false;

   public:
    Sort(InQueue &input_queue, OutQueue &output_queue,
         Batch_Pool<HitBatch> &pool)
        : m_hit_queue(input_queue),
          m_sorted_hit_queue(output_queue),
          parsehit(pool) {}

    void start_worker() {
        m_thread = std::jthread([this](std::stop_token stop) {
            pthread_setname_np(pthread_self(), "Sorter");
            while (not stop.stop_requested()) {
                if (not buffer_filled) {
                    if (m_hit_queue.front()) {
                        U64BatchPtr batch = std::move(*m_hit_queue.front());
                        std::sort(batch->begin(), batch->end(), Compare());
                        std::move(batch->begin(), batch->end(),
                                  buffer_.begin() + BATCH_SIZE);
                        m_hit_queue.pop();
                    }
                } else if (m_hit_queue.front()) {
                    U64BatchPtr batch = std::move(*m_hit_queue.front());
                    DEBUG_ASSERT(batch);

                    std::sort(batch->begin(), batch->end(), Compare());

                    std::merge(batch->begin(), batch->end(),
                               buffer_.begin() + BATCH_SIZE, buffer_.end(),
                               buffer_.begin());
                    std::move(buffer_.begin(), buffer_.begin() + BATCH_SIZE,
                              batch->begin());

                    // std::for_each(batch->begin(), batch->end(), Parse());
                    HitBatchPtr hit_batch = parsehit.parse(std::move(batch));

                    while (
                        not m_sorted_hit_queue.try_push(std::move(hit_batch))) {
                        if (stop.stop_requested()) {
                            return;
                        }
                    }
                    m_hit_queue.pop();
                }
            }
        });
    }
};
