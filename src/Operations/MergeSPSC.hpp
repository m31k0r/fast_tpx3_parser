#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <libassert/assert.hpp>

#include "QueueUtility/QueueMessage.hpp"

template <class InQueue, size_t N, typename T>
class SPSCs2SPSC {
    static_assert(N > 0);
    using Message = QueueMessage<T>;
    std::optional<Message> current;
    std::array<InQueue, N>& in;
    std::array<bool, N> ended{};
    size_t sequence_number = 0;
    bool eof_emitted = false;

   public:
    SPSCs2SPSC(std::array<InQueue, N>& queues) : in(queues) {}

    Message* front() {
        if (current) return &*current;
        if (eof_emitted) return nullptr;

        for (size_t i = 0; i < N; ++i) {
            if (not ended[i]) {
                auto* message = in[i].front();
                if (message && std::holds_alternative<EndOfFile>(*message)) {
                    in[i].pop();
                    ended[i] = true;
                }
            }
        }
        if (std::all_of(ended.begin(), ended.end(), [](bool e) { return e; })) {
            current.emplace(EndOfFile{});
            eof_emitted = true;
            return &*current;
        }

        const auto index = sequence_number % N;
        if (ended[index]) {
            for (size_t i = 0; i < N; ++i) {
                auto* message = in[i].front();
                ASSERT(not message || std::holds_alternative<EndOfFile>(*message),
                       "Missing trigger batch sequence");
            }
            return nullptr;
        }
        auto* message = in[index].front();
        if (not message) return nullptr;
        auto& batch = std::get<T>(*message);
        ASSERT(batch && batch->sequence_number == sequence_number,
               "Unexpected trigger batch sequence");
        current.emplace(std::move(batch));
        in[index].pop();
        ++sequence_number;
        return &*current;
    }

    void pop() { current.reset(); }

    size_t size() {
        size_t count = current ? 1 : 0;
        for (auto& queue : in) count += queue.size();
        return count;
    }
};
