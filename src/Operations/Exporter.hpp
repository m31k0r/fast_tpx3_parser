#pragma once
#include <fstream>
#include <iostream>
#include <stop_token>
#include <thread>
#include <variant>
#include "Batch/Batch.hpp"
#include "QueueUtility/QueueConcepts.hpp"
#include "QueueUtility/QueueMessage.hpp"
#include "Utility/AdaptivBackoff.hpp"

template <typename Queue, typename BatchPtr>
class Exporter {
    size_t counter_batch = 0;
    size_t counter = 0;
    std::jthread m_thread;
    bool received_flush = false;
    std::ofstream outfile;

    void consume(const BatchPtr& batch) {
        ++counter_batch;
        counter += batch->size;
        if (batch->size != BATCH_SIZE) {
            received_flush = true;
        } else {
            received_flush = false;
        }
        for (size_t i = 0; i < batch->size; ++i) {
            outfile << batch->operator[](i) << "\n";
        }
    }

   public:
    std::stop_token get_stop_token() { return m_thread.get_stop_token(); }
    bool operator()(Queue& queue) {
        if constexpr (SCQueueable<Queue, QueueMessage<BatchPtr>>) {
            if (auto* message = queue.front()) {
                if (std::holds_alternative<EndOfFile>(*message)) {
                    queue.pop();
                    return true;
                }
                consume(std::get<BatchPtr>(*message));
                queue.pop();
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
        while (not received_flush) {
            backoff.pause();
        }
    }
    ~Exporter() {
        std::cout << "Received " << counter_batch << " batches with " << counter
                  << " events\n";
    }
    Exporter() : outfile("export.csv") {}
};
