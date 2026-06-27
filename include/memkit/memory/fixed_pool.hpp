#pragma once

#include "../config.hpp"
#include "../detail/utility.hpp"
#include "fixed_buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace memkit::memory {

/*
 * Fixed-capacity object pool with free-list and used-bit tracking.
 * Storage may be caller-owned (fixed_buffer) or arena/heap-backed.
 */
template<typename StorageBacking>
class fixed_pool {
public:
    fixed_pool() noexcept = default;

    [[nodiscard]] status init(
        StorageBacking storage,
        std::uint32_t* free_stack,
        std::size_t    free_stack_capacity,
        std::byte*     used_bits,
        std::size_t    used_bits_bytes,
        std::size_t    elem_size,
        std::size_t    capacity
    ) noexcept
    {
        if (elem_size == 0u || capacity == 0u) {
            return status::invalid;
        }
        if (storage.data() == nullptr || free_stack == nullptr || used_bits == nullptr) {
            return status::null_ptr;
        }
        if (storage.size() < elem_size * capacity) {
            return status::invalid;
        }
        if (free_stack_capacity < capacity) {
            return status::invalid;
        }
        const std::size_t required_bits = (capacity + 7u) / 8u;
        if (used_bits_bytes < required_bits) {
            return status::invalid;
        }

        storage_     = storage;
        free_stack_  = free_stack;
        used_bits_   = used_bits;
        elem_size_   = elem_size;
        capacity_    = capacity;
        used_count_  = 0u;
        free_count_  = capacity;

        std::memset(used_bits_, 0, used_bits_bytes);
        for (std::size_t i = 0u; i < capacity; ++i) {
            free_stack_[i] = static_cast<std::uint32_t>(capacity - 1u - i);
        }

        return status::ok;
    }

    [[nodiscard]] status alloc(void** out_elem) noexcept
    {
        if (out_elem == nullptr) {
            return status::null_ptr;
        }
        *out_elem = nullptr;

        if (free_count_ == 0u) {
            return status::full;
        }

        const std::uint32_t index = free_stack_[--free_count_];
        used_bits_[index / 8u] |= static_cast<std::byte>(1u << (index % 8u));
        ++used_count_;

        *out_elem = storage_.data() + index * elem_size_;
        return status::ok;
    }

    [[nodiscard]] status free(void* elem) noexcept
    {
        if (elem == nullptr) {
            return status::null_ptr;
        }
        if (!contains(elem)) {
            return status::not_found;
        }

        const auto* const byte_ptr = static_cast<const std::byte*>(elem);
        const std::size_t index =
            static_cast<std::size_t>(byte_ptr - storage_.data()) / elem_size_;

        used_bits_[index / 8u] &= static_cast<std::byte>(~(1u << (index % 8u)));
        free_stack_[free_count_++] = static_cast<std::uint32_t>(index);
        --used_count_;
        return status::ok;
    }

    [[nodiscard]] bool contains(const void* elem) const noexcept
    {
        if (elem == nullptr || storage_.data() == nullptr || elem_size_ == 0u) {
            return false;
        }

        const auto* const byte_ptr = static_cast<const std::byte*>(elem);
        if (byte_ptr < storage_.data()) {
            return false;
        }

        const std::size_t offset =
            static_cast<std::size_t>(byte_ptr - storage_.data());
        if (offset % elem_size_ != 0u) {
            return false;
        }

        const std::size_t index = offset / elem_size_;
        if (index >= capacity_) {
            return false;
        }

        return (used_bits_[index / 8u] & static_cast<std::byte>(1u << (index % 8u))) !=
               std::byte{0};
    }

    [[nodiscard]] std::size_t size() const noexcept { return used_count_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t available() const noexcept { return free_count_; }
    [[nodiscard]] std::size_t elem_size() const noexcept { return elem_size_; }

private:
    StorageBacking storage_{};
    std::uint32_t* free_stack_  = nullptr;
    std::byte*     used_bits_   = nullptr;
    std::size_t    elem_size_   = 0u;
    std::size_t    capacity_    = 0u;
    std::size_t    used_count_  = 0u;
    std::size_t    free_count_  = 0u;
};

[[nodiscard]] constexpr std::size_t pool_used_bits_bytes(std::size_t capacity) noexcept
{
    return (capacity + 7u) / 8u;
}

[[nodiscard]] constexpr std::size_t pool_free_stack_bytes(std::size_t capacity) noexcept
{
    return capacity * sizeof(std::uint32_t);
}

[[nodiscard]] constexpr std::size_t pool_storage_bytes(
    std::size_t elem_size,
    std::size_t capacity
) noexcept
{
    return elem_size * capacity;
}

} // namespace memkit::memory
