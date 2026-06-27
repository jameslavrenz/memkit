#pragma once

#include "../status.hpp"
#include "element_policy.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace memkit::detail {

template<typename Policy>
class double_buffer_core {
public:
    double_buffer_core() = default;

    [[nodiscard]] status init(
        Policy policy,
        std::byte* slot_storage,
        std::size_t slot_capacity
    ) noexcept
    {
        if (slot_storage == nullptr || slot_capacity == 0u) {
            return status::invalid;
        }

        policy_        = policy;
        slot_storage_  = slot_storage;
        slot_capacity_ = slot_capacity;
        readable_.store(0u, std::memory_order_relaxed);
        return status::ok;
    }

    void reset_state() noexcept
    {
        policy_        = Policy{};
        slot_storage_  = nullptr;
        slot_capacity_ = 0u;
        readable_.store(0u, std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t slot_capacity() const noexcept { return slot_capacity_; }

    [[nodiscard]] void* write_slot() noexcept
    {
        const std::uint8_t read_index = readable_.load(std::memory_order_acquire);
        return slot_ptr(static_cast<std::uint8_t>(1u - read_index));
    }

    [[nodiscard]] const void* read_slot() const noexcept
    {
        return slot_ptr(readable_.load(std::memory_order_acquire));
    }

    void publish() noexcept
    {
        const std::uint8_t write_index =
            static_cast<std::uint8_t>(1u - readable_.load(std::memory_order_relaxed));
        readable_.store(write_index, std::memory_order_release);
    }

private:
    [[nodiscard]] std::size_t slot_bytes() const noexcept
    {
        return slot_capacity_ * policy_.elem_size();
    }

    [[nodiscard]] void* slot_ptr(std::uint8_t index) noexcept
    {
        return slot_storage_ + (static_cast<std::size_t>(index) * slot_bytes());
    }

    [[nodiscard]] const void* slot_ptr(std::uint8_t index) const noexcept
    {
        return slot_storage_ + (static_cast<std::size_t>(index) * slot_bytes());
    }

    Policy                   policy_{};
    std::byte*               slot_storage_  = nullptr;
    std::size_t              slot_capacity_ = 0u;
    std::atomic<std::uint8_t> readable_{0u};
};

} // namespace memkit::detail
