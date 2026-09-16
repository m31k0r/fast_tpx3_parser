#include <array>
#include <cstring>
#include <optional>

template <typename T, std::size_t Capacity, typename Compare = std::less<T>>
class SortingBuffer {
   public:
    std::optional<T> pushpop(T value) {
        std::optional<T> ret = std::nullopt;
        // pop if full
        if (size_ >= Capacity) {
            ret = m_buffer[0];
        }

        // Find insertion position.
        auto pos_iter = std::lower_bound(m_buffer.begin(), m_buffer.end(),
                                         value, Compare{});
        auto pos = std::distance(m_buffer.begin(), pos_iter);

        // Move everything until pos to zero.
        if (pos < size_) {
            std::memmove(m_buffer.data(), m_buffer.data() + 1, pos * sizeof(T));
        }
        m_buffer[pos] = value;
        ++size_;
        return ret;
    }

   private:
    std::array<T, Capacity> m_buffer{};
    std::size_t size_ = 0;
};
