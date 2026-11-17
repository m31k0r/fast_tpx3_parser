#pragma once
#include <array>

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