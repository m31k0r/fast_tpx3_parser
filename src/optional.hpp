#pragma once

// #include <stdx/functional.hpp>
// #include <stdx/type_traits.hpp>
// #include <stdx/utility.hpp>

#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace stdx {
template <typename T, typename = void>
struct tombstone_traits {
    using unspecialized = int;
    constexpr auto operator()() const {
        static_assert(
            false,
            "To use stdx::optional you must specialize stdx::tombstone_traits");
    }
};

template <typename T>
struct tombstone_traits<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    constexpr auto operator()() const {
        return std::numeric_limits<T>::infinity();
    }
};

template <typename T>
struct tombstone_traits<T, std::enable_if_t<std::is_pointer_v<T>>> {
    constexpr auto operator()() const { return nullptr; }
};

template <typename T>
struct tombstone_traits<T, std::enable_if_t<std::is_integral_v<T>>> {
    constexpr auto operator()() const { return T{}; }
};

template <typename T, typename TS = tombstone_traits<T>>
class optional {
    constexpr static inline auto traits = TS{};
    using check_specialization_t [[maybe_unused]] = decltype(traits());
    T val{traits()};

   public:
    using value_type = T;

    constexpr optional() = default;
    constexpr explicit optional(std::nullopt_t) {}

    template <typename... Args>
    constexpr explicit optional(std::in_place_t, Args &&...args)
        : val{std::forward<Args>(args)...} {}

    template <typename U = T>
        requires(std::is_constructible_v<T, U &&> and
                 not std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> and
                 not std::is_same_v<std::remove_cvref_t<U>, optional>)
    constexpr explicit optional(U &&u) : val{std::forward<U>(u)} {}

    constexpr auto operator=(std::nullopt_t) -> optional & {
        reset();
        return *this;
    }

    template <typename U = T>
        requires(std::is_constructible_v<T, U> and
                 std::is_assignable_v<T &, U> and
                 not std::is_same_v<std::remove_cvref_t<U>, optional> and
                 (std::is_scalar_v<T> or
                  not std::is_same_v<std::decay_t<U>, T>))
    constexpr auto operator=(U &&u) -> optional & {
        val = std::forward<U>(u);
        return *this;
    }

    [[nodiscard]] constexpr auto has_value() const noexcept -> bool {
        return not(val == traits());
    }
    constexpr explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] constexpr auto value() & -> value_type & { return val; }
    [[nodiscard]] constexpr auto value() const & -> value_type const & {
        return val;
    }
    [[nodiscard]] constexpr auto value() && -> value_type && {
        return std::move(val);
    }
    [[nodiscard]] constexpr auto value() const && -> value_type const && {
        return std::move(val);
    }

    [[nodiscard]] constexpr auto operator->() const -> value_type const * {
        return std::addressof(val);
    }
    [[nodiscard]] constexpr auto operator->() -> value_type * {
        return std::addressof(val);
    }

    [[nodiscard]] constexpr auto operator*() const & -> decltype(auto) {
        return value();
    }
    [[nodiscard]] constexpr auto operator*() & -> decltype(auto) {
        return value();
    }
    [[nodiscard]] constexpr auto operator*() const && -> decltype(auto) {
        return std::move(*this).value();
    }
    [[nodiscard]] constexpr auto operator*() && -> decltype(auto) {
        return std::move(*this).value();
    }

    template <typename U>
    [[nodiscard]] constexpr auto value_or(
        U &&default_value) const & -> value_type {
        return has_value() ? val : T{std::forward<U>(default_value)};
    }
    template <typename U>
    [[nodiscard]] constexpr auto value_or(U &&default_value) && -> value_type {
        return has_value() ? std::move(val) : T{std::forward<U>(default_value)};
    }

    template <typename... Args>
    constexpr auto emplace(Args &&...args) -> value_type & {
        val.~value_type();
        new (std::addressof(val)) value_type(std::forward<Args>(args)...);
        return value();
    }

    constexpr auto reset() {
        val.~value_type();
        new (std::addressof(val)) value_type(traits());
    }
};

}  // namespace stdx
