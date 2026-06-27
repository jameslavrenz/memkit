#pragma once

#include "../status.hpp"

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

struct ring_config;

namespace memkit::detail {

template<typename T>
struct typed_element_policy {
    [[nodiscard]] std::size_t elem_size() const noexcept { return sizeof(T); }
    [[nodiscard]] std::size_t alignment() const noexcept { return alignof(T); }

    void copy_construct(void* dst, const void* src) const
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(dst, src, sizeof(T));
        } else {
            new (dst) T(*static_cast<const T*>(src));
        }
    }

    void move_construct(void* dst, void* src) const
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(dst, src, sizeof(T));
        } else {
            new (dst) T(std::move(*static_cast<T*>(src)));
        }
    }

    void copy_assign(void* dst, const void* src) const { copy_construct(dst, src); }

    void destroy(void* elem) const noexcept
    {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            static_cast<T*>(elem)->~T();
        }
    }

    [[nodiscard]] bool needs_destroy_on_clear() const noexcept
    {
        return !std::is_trivially_destructible_v<T>;
    }
};

struct runtime_element_policy {
    std::size_t elem_size_ = 0u;
    std::size_t alignment_ = alignof(std::max_align_t);

    using copy_fn = status (*)(void* dst, const void* src, void* user);
    using destroy_fn = void (*)(void* elem, void* user);

    copy_fn     copy_fn_     = nullptr;
    destroy_fn  destroy_fn_  = nullptr;
    void*       user_        = nullptr;

    runtime_element_policy() = default;

    runtime_element_policy(
        std::size_t elem_size,
        copy_fn copy,
        destroy_fn destroy,
        void* user
    ) noexcept
        : elem_size_(elem_size)
        , copy_fn_(copy)
        , destroy_fn_(destroy)
        , user_(user)
    {
        if (elem_size_ > 0u) {
            const std::size_t align = elem_size_ & (~elem_size_ + 1u);
            alignment_ = align > 0u ? align : 1u;
        }
    }

    [[nodiscard]] std::size_t elem_size() const noexcept { return elem_size_; }
    [[nodiscard]] std::size_t alignment() const noexcept { return alignment_; }

    [[nodiscard]] status copy_construct(void* dst, const void* src) const
    {
        if (copy_fn_ != nullptr) {
            return copy_fn_(dst, src, user_);
        }
        std::memcpy(dst, src, elem_size_);
        return status::ok;
    }

    void copy_assign(void* dst, const void* src) const
    {
        if (copy_fn_ != nullptr) {
            (void)copy_fn_(dst, src, user_);
            return;
        }
        std::memcpy(dst, src, elem_size_);
    }

    void destroy(void* elem) const noexcept
    {
        if (destroy_fn_ != nullptr) {
            destroy_fn_(elem, user_);
        }
    }

    [[nodiscard]] bool needs_destroy_on_clear() const noexcept
    {
        return destroy_fn_ != nullptr;
    }
};

} // namespace memkit::detail
