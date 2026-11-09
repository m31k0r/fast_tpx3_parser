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
#include "QueueUtility/QueueMessage.hpp"
#include "Utility/AdaptivBackoff.hpp"

template <typename value_type, std::size_t BATCH_SIZE>
class Buffer : public std::array<value_type, BATCH_SIZE * 2> {
    using iterator = typename std::array<value_type, BATCH_SIZE * 2>::iterator;
    using const_iterator =
        typename std::array<value_type, BATCH_SIZE * 2>::const_iterator;
    using reverse_iterator =
        typename std::array<value_type, BATCH_SIZE * 2>::reverse_iterator;
    using const_reverse_iterator =
        typename std::array<value_type, BATCH_SIZE * 2>::const_reverse_iterator;

   public:
    // Iterators
    iterator middle() noexcept { return this->begin() + BATCH_SIZE; }
    const_iterator middle() const noexcept {
        return this->begin() + BATCH_SIZE;
    }
    const_iterator cmiddle() const noexcept {
        return this->cbegin() + BATCH_SIZE;
    }
};

inline auto Compare = [](PreParsed &a, PreParsed &b) { return a.toa < b.toa; };
template <typename InQueue, typename OutQueue>
    requires SCQueueable<InQueue, QueueMessage<U64BatchPtr>> &&
             SCQueueable<OutQueue, QueueMessage<HitBatchPtr>>
class Sort {
    int in_out = 0;
    InQueue &m_hit_queue;
    OutQueue &m_sorted_hit_queue;
    ParseHit parsehit;
    Batch_Pool<HitBatch> &pool;

    Buffer<PreParsed, BATCH_SIZE> buffer_;
    std::array<PreParsed, BATCH_SIZE> buffer2_;
    bool buffer_filled = false;
    std::jthread m_thread;

   public:
    ~Sort() {
        if (m_thread.joinable()) {
            m_thread.request_stop();
            m_thread.join();
        }
        std::cout << in_out << " == 0 " << std::endl;
    }
    Sort(InQueue &input_queue, OutQueue &output_queue,
         Batch_Pool<HitBatch> &pool, Camera &camera, uint8_t chip = 0)
        : m_hit_queue(input_queue),
          m_sorted_hit_queue(output_queue),
          parsehit(pool, camera, chip),
          pool(pool) {}

    void sort_emit(U64BatchPtr batch, std::stop_token stop) {
        size_t n = batch->size;
        std::transform(
            batch->begin(), batch->end(), buffer2_.begin(),
            [this](uint64_t t_data) { return parsehit.parse(t_data); });
        std::sort(buffer2_.begin(), buffer2_.begin() + n, Compare);

        std::merge(buffer2_.begin(), buffer2_.begin() + n, buffer_.middle(),
                   buffer_.end(), buffer_.begin(), Compare);

        auto hit_batch =
            parsehit.parse_no_toa(buffer_.begin(), buffer_.middle());
        std::transform(buffer_.begin(), buffer_.middle(),
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
    void fill_buffer(U64BatchPtr batch, std::stop_token stop) {
        in_out += batch->size;
        if (batch->size == BATCH_SIZE) {
            std::transform(
                batch->begin(), batch->end(), buffer_.middle(),
                [this](uint64_t t_data) { return parsehit.parse(t_data); });
            std::sort(buffer_.middle(), buffer_.end(), Compare);
            buffer_filled = true;
            std::cout << "Buffer is filled" << std::endl;
        } else {
            const auto n = batch->size;
            std::transform(
                batch->begin(), batch->end(), buffer2_.begin(),
                [this](uint64_t t_data) { return parsehit.parse(t_data); });
            std::sort(buffer2_.begin(), buffer2_.begin() + n, Compare);
            auto hit_batch =
                parsehit.parse_no_toa(buffer2_.begin(), buffer2_.begin() + n);
            std::transform(buffer2_.begin(), buffer2_.begin() + n,
                           hit_batch->toa.begin(),
                           [](auto b) { return b.toa; });
            ASSERT(hit_batch->size == n);
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
    }

    void start_worker() {
        m_thread = std::jthread([this](std::stop_token stop) {
            pthread_setname_np(pthread_self(), "Sorter");
            while (not stop.stop_requested()) {
                if (m_hit_queue.front()) {
                    auto message = std::move(*m_hit_queue.front());
                    m_hit_queue.pop();
                    if (std::holds_alternative<EndOfFile>(message)) {
                        if (buffer_filled) {
                            auto hit_batch = parsehit.parse_no_toa(
                                buffer_.middle(), buffer_.end());
                            std::transform(buffer_.middle(), buffer_.end(),
                                           hit_batch->toa.begin(),
                                           [](auto b) { return b.toa; });
                            in_out -= hit_batch->size;
                            adaptive_backoff backoff;
                            while (not m_sorted_hit_queue.try_push(
                                std::move(hit_batch))) {
                                backoff.pause();
                                if (stop.stop_requested()) return;
                            }
                            buffer_filled = false;
                        }
                        adaptive_backoff backoff;
                        while (not m_sorted_hit_queue.try_push(EndOfFile{})) {
                            backoff.pause();
                            if (stop.stop_requested()) return;
                        }
                        return;
                    }
                    U64BatchPtr batch =
                        std::move(std::get<U64BatchPtr>(message));
                    DEBUG_ASSERT(batch);
                    const auto batch_size = batch->size;
                    if (batch_size == 0) continue;

                    if (not buffer_filled) {
                        fill_buffer(std::move(batch), stop);
                    } else {
                        in_out += batch->size;
                        if (batch->size == BATCH_SIZE)
                            sort_emit(std::move(batch), stop);
                        else {
                            sort_emit(std::move(batch), stop);
                            buffer_filled = false;

                            auto hit_batch = parsehit.parse_no_toa(
                                buffer_.middle(),
                                buffer_.middle() + batch_size);
                            std::transform(buffer_.middle(),
                                           buffer_.middle() + batch_size,
                                           hit_batch->toa.begin(),
                                           [](auto b) { return b.toa; });
                            DEBUG_ASSERT(hit_batch->size == batch_size);
                            adaptive_backoff backoff;
                            in_out -= hit_batch->size;
                            DEBUG_ASSERT(in_out >= 0);
                            while (not m_sorted_hit_queue.try_push(
                                std::move(hit_batch))) {
                                backoff.pause();
                                if (stop.stop_requested()) {
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        });
    }
};
