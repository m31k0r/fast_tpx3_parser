#pragma once
#include <concepts>
#include <utility>

template <typename Q, typename T>
concept SCQueueable = requires(Q& queue, T& value) {
    { queue.front() } -> std::convertible_to<T*>;
    { queue.pop() } -> std::convertible_to<void>;
};

template <typename Q, typename T>
concept InQueueable = requires(Q& queue, T& value) {
    { queue.try_push(std::move(value)) } -> std::convertible_to<bool>;
};
