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

auto Compare = [](PreParsed &a, PreParsed &b) { return a.toa < b.toa; };
template <typename InQueue, typename OutQueue>
    requires SCQueueable<InQueue, U64BatchPtr> &&
             SCQueueable<OutQueue, HitBatchPtr>
class Sort {
    int in_out = 0;
    InQueue &m_hit_queue;
    OutQueue &m_sorted_hit_queue;
    ParseHit parsehit;
    Batch_Pool<HitBatch> &pool;

    std::jthread m_thread;

    std::array<PreParsed, BATCH_SIZE * 2> buffer_;
    std::array<PreParsed, BATCH_SIZE> buffer2_;
    bool buffer_filled = false;

   public:
    ~Sort() { std::cout << in_out << " == 0 " << std::endl; }
    Sort(InQueue &input_queue, OutQueue &output_queue,
         Batch_Pool<HitBatch> &pool, Camera &camera)
        : m_hit_queue(input_queue),
          m_sorted_hit_queue(output_queue),
          parsehit(pool, camera),
          pool(pool) {}

    void sort_emit(U64BatchPtr batch, std::stop_token stop) {
        size_t n = batch->size;
        std::transform(
            batch->begin(), batch->end(), buffer2_.begin(),
            [this](uint64_t t_data) { return parsehit.parse(t_data); });
        std::sort(buffer2_.begin(), buffer2_.begin() + n, Compare);

        std::merge(buffer2_.begin(), buffer2_.begin() + n,
                   buffer_.begin() + BATCH_SIZE, buffer_.end(), buffer_.begin(),
                   Compare);

        auto hit_batch = parsehit.parse_no_toa(buffer_.begin(),
                                               buffer_.begin() + BATCH_SIZE);
        std::transform(buffer_.begin(), buffer_.begin() + BATCH_SIZE,
                       hit_batch->toa.begin(), [](auto b) { return b.toa; });

        adaptive_backoff backoff;
        in_out -= hit_batch->size;
        ASSERT(in_out >= 0);
        while (not m_sorted_hit_queue.try_push(std::move(hit_batch))) {
            backoff.pause();
            if (stop.stop_requested()) {
                return;
            }
        }
    }
    void start_worker() {
        m_thread = std::jthread([this](std::stop_token stop) {
            pthread_setname_np(pthread_self(), "Sorter");
            while (not stop.stop_requested()) {
                if (not buffer_filled) {
                    if (m_hit_queue.front()) {
                        U64BatchPtr batch = std::move(*m_hit_queue.front());

                        in_out += batch->size;
                        if (batch->size == BATCH_SIZE) {
                            std::transform(batch->begin(), batch->end(),
                                           buffer_.begin() + BATCH_SIZE,
                                           [this](uint64_t t_data) {
                                               return parsehit.parse(t_data);
                                           });
                            std::sort(buffer_.begin() + BATCH_SIZE,
                                      buffer_.end(), Compare);
                            m_hit_queue.pop();
                            buffer_filled = true;
                            std::cout << "Buffer is filled" << std::endl;
                        } else {
                            const auto n = batch->size;
                            std::transform(batch->begin(), batch->end(),
                                           buffer2_.begin(),
                                           [this](uint64_t t_data) {
                                               return parsehit.parse(t_data);
                                           });
                            std::sort(buffer2_.begin(), buffer2_.begin() + n,
                                      Compare);
                            auto hit_batch = parsehit.parse_no_toa(
                                buffer2_.begin(), buffer2_.begin() + n);
                            std::transform(buffer2_.begin(),
                                           buffer2_.begin() + n,
                                           hit_batch->toa.begin(),
                                           [](auto b) { return b.toa; });
                            ASSERT(hit_batch->size == n);
                            adaptive_backoff backoff;
                            in_out -= hit_batch->size;
                            ASSERT(in_out >= 0);
                            while (not m_sorted_hit_queue.try_push(
                                std::move(hit_batch))) {
                                backoff.pause();
                                if (stop.stop_requested()) {
                                    return;
                                }
                            }
                        }
                    }
                } else if (m_hit_queue.front()) {
                    U64BatchPtr batch = std::move(*m_hit_queue.front());
                    in_out += batch->size;
                    DEBUG_ASSERT(batch);
                    if (batch->size == BATCH_SIZE)
                        sort_emit(std::move(batch), stop);
                    else {
                        const auto n = batch->size;
                        sort_emit(std::move(batch), stop);
                        buffer_filled = false;

                        auto hit_batch = parsehit.parse_no_toa(
                            buffer_.begin() + BATCH_SIZE,
                            buffer_.begin() + BATCH_SIZE + n);
                        std::transform(buffer_.begin() + BATCH_SIZE,
                                       buffer_.begin() + BATCH_SIZE + n,
                                       hit_batch->toa.begin(),
                                       [](auto b) { return b.toa; });
                        ASSERT(hit_batch->size == n);
                        adaptive_backoff backoff;
                        in_out -= hit_batch->size;
                        ASSERT(in_out >= 0);
                        while (not m_sorted_hit_queue.try_push(
                            std::move(hit_batch))) {
                            backoff.pause();
                            if (stop.stop_requested()) {
                                return;
                            }
                        }
                    }
                    m_hit_queue.pop();
                }
            }
        });
    }
};
