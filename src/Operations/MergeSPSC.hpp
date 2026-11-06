#pragma once

#include <array>
#include <cstddef>
#include <iostream>
#include <print>

template <class InQueue, size_t N, typename T>
class SPSCs2SPSC {
    T current_batch;
    std::array<InQueue, N>& in;
    size_t sequence_number = 0;

    T next_batch() {
        auto front = in[sequence_number % N].front();
        if (not front) {
            return nullptr;
        }

        const auto front_seq_nr = front->get()->sequence_number;
        if (front_seq_nr == sequence_number) {
            T batch = std::move(*front);
            in[sequence_number % N].pop();
            sequence_number++;
            return batch;
        }

        while (in[sequence_number % N].front()) {
            auto val = std::move(*in[sequence_number % N].front());
            std::cout << val->sequence_number << std::endl;
            in[sequence_number % N].pop();
        }
        PANIC(std::format("No match found but queue is not empty {}!={}",
                          front_seq_nr, sequence_number));
        return nullptr;
    }

   public:
    SPSCs2SPSC(std::array<InQueue, N>& queues) : in(queues) {}

    T* front() {
        if (not current_batch) {
            current_batch = next_batch();
        }
        if (not current_batch) {
            return nullptr;
        }
        return &current_batch;
    };
    void pop() {
        // DEBUG_ASSERT(current_batch,
        //              "Should not pop if current_batch is nullptr");
        current_batch = nullptr;
    };

    void print_state() {
        std::print("Seq: {}  -  ", sequence_number);
        for (auto& queue : in) {
            auto front = queue.front();
            if (not front) {
                std::print("Empty; ");
                continue;
            }
            std::print("S:{}L:{}", front->get()->sequence_number, queue.size());
        }
        std::println();
    };
    size_t size() {
        auto min = std::min_element(
            in.begin(), in.end(),
            [](auto& e1, auto& e2) { return e1.size() < e2.size(); });
        return min->size();
    }
};
