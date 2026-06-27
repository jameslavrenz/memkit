#pragma once

#include "../status.hpp"
#include "element_policy.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace memkit::detail {

enum class handle_pool_storage_kind : std::uint8_t {
    external = 0,
    owns     = 1u << 0,
    arena    = 1u << 1,
    heap     = 1u << 2,
};

[[nodiscard]] inline handle_pool_storage_kind operator|(
    handle_pool_storage_kind a,
    handle_pool_storage_kind b
) noexcept
{
    return static_cast<handle_pool_storage_kind>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

class handle_pool_core {
public:
    static constexpr std::uint32_t invalid_handle = 0u;
    static constexpr unsigned        index_bits     = 16u;
    static constexpr std::uint32_t index_mask     = (1u << index_bits) - 1u;

    handle_pool_core() = default;

    [[nodiscard]] static constexpr std::size_t generations_bytes(std::size_t capacity) noexcept
    {
        return capacity * sizeof(std::uint16_t);
    }

    [[nodiscard]] static constexpr std::size_t free_stack_bytes(std::size_t capacity) noexcept
    {
        return capacity * sizeof(std::uint32_t);
    }

    [[nodiscard]] status init(
        std::byte* storage,
        std::uint16_t* generations,
        std::uint32_t* free_stack,
        std::size_t capacity,
        std::size_t elem_size,
        std::size_t elem_alignment
    ) noexcept
    {
        if (storage == nullptr || generations == nullptr || free_stack == nullptr ||
            capacity == 0u || elem_size == 0u) {
            return status::invalid;
        }
        if ((capacity - 1u) > index_mask) {
            return status::invalid;
        }

        storage_      = storage;
        generations_  = generations;
        free_stack_   = free_stack;
        capacity_     = capacity;
        elem_size_    = elem_size;
        elem_align_   = elem_alignment > 0u ? elem_alignment : 1u;
        live_count_   = 0u;
        free_count_   = capacity;
        storage_kind_ = handle_pool_storage_kind::external;
        return reset_free_list();
    }

    void reset_state() noexcept
    {
        storage_      = nullptr;
        generations_  = nullptr;
        free_stack_   = nullptr;
        capacity_     = 0u;
        elem_size_    = 0u;
        elem_align_   = 0u;
        live_count_   = 0u;
        free_count_   = 0u;
        storage_kind_ = handle_pool_storage_kind::external;
    }

    [[nodiscard]] std::size_t size() const noexcept { return live_count_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t available() const noexcept { return free_count_; }
    [[nodiscard]] bool empty() const noexcept { return live_count_ == 0u; }
    [[nodiscard]] bool full() const noexcept { return free_count_ == 0u; }

    [[nodiscard]] static std::uint32_t make_handle(std::uint16_t index, std::uint16_t generation) noexcept
    {
        return (static_cast<std::uint32_t>(generation) << index_bits) | static_cast<std::uint32_t>(index);
    }

    [[nodiscard]] static std::uint16_t handle_index(std::uint32_t handle) noexcept
    {
        return static_cast<std::uint16_t>(handle & index_mask);
    }

    [[nodiscard]] static std::uint16_t handle_generation(std::uint32_t handle) noexcept
    {
        return static_cast<std::uint16_t>(handle >> index_bits);
    }

    [[nodiscard]] status acquire(void** out_ptr, std::uint32_t* out_handle) noexcept
    {
        if (out_ptr == nullptr || out_handle == nullptr) {
            return status::null_ptr;
        }
        *out_ptr    = nullptr;
        *out_handle = invalid_handle;

        if (free_count_ == 0u) {
            return status::full;
        }

        const std::uint32_t index = free_stack_[--free_count_];
        if (index >= capacity_) {
            return status::invalid;
        }

        if (generations_[index] == 0u) {
            generations_[index] = 1u;
        }

        *out_ptr    = slot_ptr(index);
        *out_handle = make_handle(static_cast<std::uint16_t>(index), generations_[index]);
        ++live_count_;
        return status::ok;
    }

    [[nodiscard]] status release(std::uint32_t handle) noexcept
    {
        if (handle == invalid_handle) {
            return status::invalid;
        }

        const std::uint16_t index = handle_index(handle);
        const std::uint16_t generation = handle_generation(handle);
        if (index >= capacity_ || generations_[index] != generation) {
            return status::invalid;
        }

        bump_generation(index);
        free_stack_[free_count_++] = index;
        --live_count_;
        return status::ok;
    }

    [[nodiscard]] status get(std::uint32_t handle, void** out_ptr) const noexcept
    {
        if (out_ptr == nullptr) {
            return status::null_ptr;
        }
        *out_ptr = nullptr;

        if (handle == invalid_handle) {
            return status::invalid;
        }

        const std::uint16_t index = handle_index(handle);
        const std::uint16_t generation = handle_generation(handle);
        if (index >= capacity_ || generations_[index] != generation) {
            return status::invalid;
        }

        *out_ptr = const_cast<std::byte*>(slot_ptr(index));
        return status::ok;
    }

    [[nodiscard]] bool valid(std::uint32_t handle) const noexcept
    {
        if (handle == invalid_handle) {
            return false;
        }

        const std::uint16_t index = handle_index(handle);
        const std::uint16_t generation = handle_generation(handle);
        return index < capacity_ && generations_[index] == generation;
    }

    [[nodiscard]] handle_pool_storage_kind storage_kind() const noexcept { return storage_kind_; }
    void set_storage_kind(handle_pool_storage_kind kind) noexcept { storage_kind_ = kind; }

    [[nodiscard]] std::byte* storage() const noexcept { return storage_; }
    [[nodiscard]] std::uint16_t* generations() const noexcept { return generations_; }
    [[nodiscard]] std::uint32_t* free_stack() const noexcept { return free_stack_; }

private:
    [[nodiscard]] status reset_free_list() noexcept
    {
        for (std::size_t i = 0u; i < capacity_; ++i) {
            generations_[i] = 0u;
            free_stack_[i]  = static_cast<std::uint32_t>(capacity_ - 1u - i);
        }
        free_count_ = capacity_;
        live_count_ = 0u;
        return status::ok;
    }

    void bump_generation(std::uint16_t index) noexcept
    {
        if (generations_[index] == 0xFFFFu) {
            generations_[index] = 1u;
        } else {
            ++generations_[index];
        }
    }

    [[nodiscard]] std::byte* slot_ptr(std::uint32_t index) noexcept
    {
        return storage_ + index * elem_size_;
    }

    [[nodiscard]] const std::byte* slot_ptr(std::uint32_t index) const noexcept
    {
        return storage_ + index * elem_size_;
    }

    std::byte*                 storage_      = nullptr;
    std::uint16_t*             generations_  = nullptr;
    std::uint32_t*             free_stack_   = nullptr;
    std::size_t                capacity_     = 0u;
    std::size_t                elem_size_    = 0u;
    std::size_t                elem_align_   = 0u;
    std::size_t                live_count_   = 0u;
    std::size_t                free_count_   = 0u;
    handle_pool_storage_kind   storage_kind_ = handle_pool_storage_kind::external;
};

} // namespace memkit::detail
