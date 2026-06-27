#pragma once

#include "../status.hpp"

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace memkit::detail {

template<typename T>
struct typed_element_ops {
    using element_type = T;

    static constexpr std::size_t size() noexcept { return sizeof(T); }
    static constexpr std::size_t alignment() noexcept { return alignof(T); }

    static void copy(T* dst, const T* src) noexcept
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(dst, src, sizeof(T));
        } else {
            new (dst) T(*src);
        }
    }

    static void move(T* dst, T* src) noexcept
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(dst, src, sizeof(T));
        } else {
            new (dst) T(std::move(*src));
        }
    }

    static void destroy(T* ptr) noexcept
    {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            ptr->~T();
        }
    }

    template<typename... Args>
    static void construct(T* ptr, Args&&... args)
    {
        new (ptr) T(std::forward<Args>(args)...);
    }
};

} // namespace memkit::detail
