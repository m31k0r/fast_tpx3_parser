#pragma once
#include <atomic>
#include <iostream>
#include <stop_token>
#include <thread>
#include <variant>
#include "Batch/Batch.hpp"
#include "QueueUtility/QueueConcepts.hpp"
#include "QueueUtility/QueueMessage.hpp"
#include "Utility/AdaptivBackoff.hpp"

template <typename Queue, typename BatchPtr>
class Sink {
    //size_t counter_batch = 0;
    //size_t counter = 0;
    std::jthread m_thread;
    std::atomic_size_t received_flushes = 0;

    void consume(const BatchPtr& batch) {
        //++counter_batch;
        //counter += batch->size;
    }

   public:
    std::stop_token get_stop_token() { return m_thread.get_stop_token(); }
    bool operator()(Queue& queue) {
        if constexpr (SCQueueable<Queue, QueueMessage<BatchPtr>>) {
            if (auto* message = queue.front()) {
                if (not message) return false;
                auto value = std::move(*message);
                queue.pop();

                return std::visit(
                    [this](auto&& item) -> bool {
                        using T = std::decay_t<decltype(item)>;
                        if constexpr (std::is_same_v<T, EndOfStream>) {
                            return true;
                        } else if constexpr (std::is_same_v<T, Flush>) {
                            received_flushes.fetch_add(1);
                        } else if constexpr (std::is_same_v<T, BatchPtr>) {
                            if (!item) return false;
                            consume(item);
                        }
                        return false;
                    },
                    value);
            }
        } else {
            if (auto* batch = queue.front()) {
                consume(*batch);
                queue.pop();
            }
        }
        return false;
    }

    void start(Queue& queue) {
        m_thread = std::jthread([this, &queue](std::stop_token stop) {
            pthread_setname_np(pthread_self(), "Sink");
            while (not stop.stop_requested()) {
                if ((*this)(queue)) return;
            }
        });
    }
    void wait() {
        if (m_thread.joinable()) m_thread.join();
    }
    void wait_for_flush() {
        adaptive_backoff backoff;
        for (;;) {
            auto count = received_flushes.load();
            if (count == 0) {
                backoff.pause();
            } else if (
                received_flushes.compare_exchange_weak(count, count - 1)) {
                return;
            }
        }
    }
    /*~Sink() {
        std::cout << "Received " << counter_batch << " batches with " << counter
                  << " events\n";
    }*/
};
