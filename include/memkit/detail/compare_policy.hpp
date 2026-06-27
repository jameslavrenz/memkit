#pragma once

#include "../status.hpp"

#include <cstddef>
#include <cstring>
#include <type_traits>

namespace memkit::detail {

template<typename T, typename = void>
struct has_less_than : std::false_type {};

template<typename T>
struct has_less_than<T, std::void_t<decltype(std::declval<const T&>() < std::declval<const T&>())>>
    : std::true_type {};

struct runtime_compare_policy {
    using compare_fn = int (*)(const void* a, const void* b, void* user);

    std::size_t elem_size_  = 0u;
    compare_fn  compare_fn_ = nullptr;
    void*       user_       = nullptr;

    runtime_compare_policy() = default;

    runtime_compare_policy(std::size_t elem_size, compare_fn compare, void* user) noexcept
        : elem_size_(elem_size)
        , compare_fn_(compare)
        , user_(user)
    {}

    [[nodiscard]] int compare(const void* a, const void* b) const noexcept
    {
        if (compare_fn_ != nullptr) {
            return compare_fn_(a, b, user_);
        }
        return std::memcmp(a, b, elem_size_);
    }
};

template<typename T>
struct type_compare {
    [[nodiscard]] int operator()(const T& a, const T& b) const noexcept
    {
        if constexpr (has_less_than<T>::value) {
            if (a < b) {
                return -1;
            }
            if (b < a) {
                return 1;
            }
            return 0;
        } else if constexpr (std::is_trivially_copyable_v<T>) {
            const int cmp = std::memcmp(&a, &b, sizeof(T));
            return cmp < 0 ? -1 : (cmp > 0 ? 1 : 0);
        } else {
            static_assert(sizeof(T) == 0, "Provide a custom Compare functor for non-trivial keys");
            return 0;
        }
    }
};

} // namespace memkit::detail
