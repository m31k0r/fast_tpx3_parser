#pragma once
#include <thread>
#include "Batch/Batch.hpp"
#include "QueueUtility/QueueConcepts.hpp"
#include "QueueUtility/QueueMessage.hpp"
#include "Utility/AdaptivBackoff.hpp"

template <class Work, class InQueue, class OutQueue>
class Worker {
    InQueue& in;
    OutQueue& out;
    Work work;
    std::jthread worker;

   public:
    Worker(Work t_work, InQueue& t_in, OutQueue& t_out)
        : in(t_in), out(t_out), work(std::move(t_work)) {
        worker = std::jthread([this](const std::stop_token& stop_token) {
            pthread_setname_np(pthread_self(), "Worker");
            uint64_t previous_seq_nr [[maybe_unused]] = 0;
            while (not stop_token.stop_requested()) {
                U64BatchPtr ba;
                if (in.front()) {
                    auto message = std::move(*in.front());
                    in.pop();
                    if (std::holds_alternative<EndOfFile>(message)) {
                        adaptive_backoff backoff;
                        while (not out.try_push(EndOfFile{})) {
                            backoff.pause();
                            if (stop_token.stop_requested()) return;
                        }
                        return;
                    }
                    ba = std::move(std::get<U64BatchPtr>(message));
                }
                if (ba) {
                    const auto old_seq [[maybe_unused]] = ba->sequence_number;
                    auto res = work.parse(std::move(ba));
                    DEBUG_ASSERT(res);
                    DEBUG_ASSERT(res->sequence_number == old_seq);
                    previous_seq_nr = old_seq;
                    adaptive_backoff backoff;
                    while (not out.try_push(std::move(res))) {
                        backoff.pause();
                        if (stop_token.stop_requested()) return;
                    }
                }
            }
        });
    }
};
