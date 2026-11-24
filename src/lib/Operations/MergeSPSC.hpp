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
    std::array<bool, N> at_new_file{};
    std::array<bool, N> at_flush{};
    std::array<std::optional<NewFile>, N> new_files{};
    size_t sequence_number = 0;
    bool eof_emitted = false;

   public:
    SPSCs2SPSC(std::array<InQueue, N>& queues) : in(queues) {}

    Message* front() {
        if (current) return &*current;
        if (eof_emitted) return nullptr;

        for (size_t i = 0; i < N; ++i) {
            if (not ended[i] && not at_new_file[i] && not at_flush[i]) {
                auto* message = in[i].front();
                if (message && std::holds_alternative<EndOfStream>(*message)) {
                    in[i].pop();
                    ended[i] = true;
                } else if (message &&
                           std::holds_alternative<NewFile>(*message)) {
                    new_files[i] = std::get<NewFile>(*message);
                    in[i].pop();
                    at_new_file[i] = true;
                } else if (message && std::holds_alternative<Flush>(*message)) {
                    in[i].pop();
                    at_flush[i] = true;
                }
            }
        }
        const bool all_ended =
            std::all_of(ended.begin(), ended.end(), [](bool e) { return e; });
        const bool all_new_file =
            std::all_of(at_new_file.begin(), at_new_file.end(),
                        [](bool boundary) { return boundary; });
        const bool all_flush =
            std::all_of(at_flush.begin(), at_flush.end(),
                        [](bool boundary) { return boundary; });
        const bool all_at_boundary = [&] {
            for (size_t i = 0; i < N; ++i) {
                if (not ended[i] && not at_new_file[i] && not at_flush[i])
                    return false;
            }
            return true;
        }();
        ASSERT(not all_at_boundary || all_ended || all_new_file || all_flush,
               "Mismatched trigger-stream boundaries");
        if (all_ended) {
            current.emplace(EndOfStream{});
            eof_emitted = true;
            return &*current;
        }
        if (all_new_file) {
            for (size_t i = 1; i < N; ++i) {
                ASSERT(new_files[i]->filename == new_files[0]->filename,
                       "Mismatched new-file boundaries");
            }
            current.emplace(*new_files[0]);
            at_new_file.fill(false);
            for (auto& file : new_files) file.reset();
            return &*current;
        }
        if (all_flush) {
            current.emplace(Flush{});
            at_flush.fill(false);
            return &*current;
        }

        const auto index = sequence_number % N;
        if (ended[index] || at_new_file[index] || at_flush[index]) {
            for (size_t i = 0; i < N; ++i) {
                auto* message = in[i].front();
                ASSERT(not message ||
                           std::holds_alternative<EndOfStream>(*message) ||
                           std::holds_alternative<NewFile>(*message) ||
                           std::holds_alternative<Flush>(*message),
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
