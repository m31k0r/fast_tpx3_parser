#pragma once

#include <algorithm>
#include <array>
#include <libassert/assert.hpp>

#include "QueueUtility/QueueMessage.hpp"

template <class InQueue, size_t N, typename T>
class SPSCs2SPSC {
    static_assert(N > 0);
    using Message = QueueMessage<T>;
    std::optional<Message> current;
    std::array<InQueue, N>& in;
    size_t sequence_number = 0;
    bool terminated = false;

   public:
    SPSCs2SPSC(std::array<InQueue, N>& queues) : in(queues) {}

    Message* front() {
        if (current) return &*current;
        if (terminated) return nullptr;

        const auto index = sequence_number % N;
        if (in[index].front() == nullptr)
            return nullptr;
        const auto boundary = queue_boundary(*in[index].front());
        if (boundary != QueueBoundary::None) {
            for (auto& queue : in) {
                if (!queue.front()) return nullptr;
                ASSERT(queue_boundary(*queue.front()) == boundary,
                       "Mismatched trigger-stream boundaries");
                if (boundary == QueueBoundary::NewFile) {
                    ASSERT(std::get<NewFile>(*queue.front()).filename ==
                               std::get<NewFile>(*in[index].front()).filename,
                           "Mismatched trigger-stream file names");
                }
            }
            current.emplace(std::move(*in[index].front()));
            for (auto& queue : in) queue.pop();
            terminated = boundary == QueueBoundary::EndOfStream;
            return &*current;
        }

        auto message = std::move(*in[index].front());
        in[index].pop();
        auto batch = std::get<T>(std::move(message));
        ASSERT(batch);
        ASSERT(batch->sequence_number == sequence_number, "Unexpected trigger batch sequence");
        current.emplace(std::move(batch));
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
