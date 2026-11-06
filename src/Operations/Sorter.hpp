#pragma once
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <libassert/assert.hpp>
#include <thread>

#include "Batch/Batch.hpp"
#include "Constants.hpp"
#include "Parsing/ParseHits.hpp"
#include "QueueUtility/QueueConcepts.hpp"
#include "Utility/AdaptivBackoff.hpp"

template <typename InQueue, typename OutQueue>
    requires SCQueueable<InQueue, U64BatchPtr> &&
             SCQueueable<OutQueue, HitBatchPtr>
class Sort {
    InQueue &m_hit_queue;
    OutQueue &m_sorted_hit_queue;
    ParseHit parsehit;
    Batch_Pool<HitBatch> &pool;

    std::jthread m_thread;

    std::array<PreParsed, BATCH_SIZE * 2> buffer_;
    std::array<PreParsed, BATCH_SIZE> buffer2_;
    bool buffer_filled = false;

   public:
    Sort(InQueue &input_queue, OutQueue &output_queue,
         Batch_Pool<HitBatch> &pool, Camera &camera)
        : m_hit_queue(input_queue),
          m_sorted_hit_queue(output_queue),
          parsehit(pool, camera),
          pool(pool) {}

    void start_worker() {
        m_thread = std::jthread([this](std::stop_token stop) {
            pthread_setname_np(pthread_self(), "Sorter");
            auto Compare = [](PreParsed &a, PreParsed &b) {
                return a.toa < b.toa;
            };
            auto emit = [this](size_t offset, size_t len,
                               std::stop_token &st) {
                auto hit_batch = parsehit.parse_no_toa(
                    buffer_.data() + offset, buffer_.data() + offset + len);
                std::transform(buffer_.begin() + offset,
                               buffer_.begin() + offset + len,
                               hit_batch->toa.begin(),
                               [](auto b) { return b.toa; });
                adaptive_backoff backoff;
                while (not m_sorted_hit_queue.try_push(std::move(hit_batch))) {
                    backoff.pause();
                    if (st.stop_requested()) {
                        return false;
                    }
                }
                return true;
            };
            while (not stop.stop_requested()) {
                if (not buffer_filled) {
                    if (m_hit_queue.front()) {
                        U64BatchPtr batch = std::move(*m_hit_queue.front());
                        const size_t n = batch->size;
                        std::transform(batch->begin(), batch->end(),
                                       buffer_.begin() + BATCH_SIZE,
                                       [this](uint64_t t_data) {
                                           return parsehit.parse(t_data);
                                       });
                        std::sort(buffer_.begin() + BATCH_SIZE,
                                  buffer_.begin() + BATCH_SIZE + n, Compare);
                        if (n == BATCH_SIZE) {
                            buffer_filled = true;
                        } else {
                            // Flush with nothing held: emit all hits
                            for (size_t off = 0; off < n; off += BATCH_SIZE) {
                                const size_t len =
                                    std::min<size_t>(BATCH_SIZE, n - off);
                                if (not emit(BATCH_SIZE + off, len, stop)) {
                                    return;
                                }
                            }
                        }
                        m_hit_queue.pop();
                    }
                } else if (m_hit_queue.front()) {
                    U64BatchPtr batch = std::move(*m_hit_queue.front());
                    DEBUG_ASSERT(batch);

                    const size_t n = batch->size;
                    std::transform(batch->begin(), batch->end(),
                                   buffer2_.begin(), [this](uint64_t t_data) {
                                       return parsehit.parse(t_data);
                                   });
                    std::sort(buffer2_.begin(), buffer2_.begin() + n, Compare);

                    // Full batch: emit the BATCH_SIZE oldest hits.
                    // Partial batch (flush): emit everything.
                    auto merge_end = std::merge(
                        buffer2_.begin(), buffer2_.begin() + n,
                        buffer_.begin() + BATCH_SIZE, buffer_.end(),
                        buffer_.begin(), Compare);
                    const size_t total = merge_end - buffer_.begin();
                    const bool flush = n < BATCH_SIZE;
                    const size_t emit_total = flush ? total : BATCH_SIZE;

                    for (size_t off = 0; off < emit_total; off += BATCH_SIZE) {
                        const size_t len =
                            std::min<size_t>(BATCH_SIZE, emit_total - off);
                        if (not emit(off, len, stop)) {
                            return;
                        }
                    }
                    if (flush) {
                        buffer_filled = false;
                    }
                    m_hit_queue.pop();
                }
            }
        });
    }
};
