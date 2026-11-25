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
    std::array<QueueBoundary, N> boundaries{};
    std::array<std::optional<NewFile>, N> new_files{};
    size_t sequence_number = 0;
    bool eof_emitted = false;

    void absorb_boundary(size_t i) {
        auto* message = in[i].front();
        if (!message || queue_boundary(*message) == QueueBoundary::None) return;
        boundaries[i] = queue_boundary(*message);
        if (boundaries[i] == QueueBoundary::NewFile)
            new_files[i] = std::get<NewFile>(*message);
        in[i].pop();
    }

   public:
    SPSCs2SPSC(std::array<InQueue, N>& queues) : in(queues) {}

    Message* front() {
        if (current) return &*current;
        if (eof_emitted) return nullptr;

        for (size_t i = 0; i < N; ++i) {
            if (boundaries[i] == QueueBoundary::None) absorb_boundary(i);
        }
        const bool all_at_boundary =
            std::all_of(boundaries.begin(), boundaries.end(), [](auto boundary) {
                return boundary != QueueBoundary::None;
            });
        const auto boundary = boundaries.front();
        ASSERT(not all_at_boundary ||
                   std::all_of(boundaries.begin(), boundaries.end(),
                               [boundary](auto b) { return b == boundary; }),
               "Mismatched trigger-stream boundaries");
        if (all_at_boundary && boundary == QueueBoundary::EndOfStream) {
            current.emplace(EndOfStream{});
            eof_emitted = true;
            return &*current;
        }
        if (all_at_boundary && boundary == QueueBoundary::NewFile) {
            for (size_t i = 1; i < N; ++i) {
                ASSERT(new_files[i]->filename == new_files[0]->filename,
                       "Mismatched new-file boundaries");
            }
            current.emplace(*new_files[0]);
            boundaries.fill(QueueBoundary::None);
            for (auto& file : new_files) file.reset();
            return &*current;
        }
        if (all_at_boundary && boundary == QueueBoundary::Flush) {
            current.emplace(Flush{});
            boundaries.fill(QueueBoundary::None);
            return &*current;
        }

        const auto index = sequence_number % N;
        if (boundaries[index] != QueueBoundary::None) {
            for (size_t i = 0; i < N; ++i) {
                auto* message = in[i].front();
                ASSERT(not message || queue_boundary(*message) != QueueBoundary::None,
                       "Missing trigger batch sequence");
            }
            return nullptr;
        }
        auto* message = in[index].front();
        if (not message) return nullptr;
        if (queue_boundary(*message) != QueueBoundary::None) {
            // A boundary arrived after the scan above; absorb it and re-evaluate.
            absorb_boundary(index);
            return front();
        }
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
