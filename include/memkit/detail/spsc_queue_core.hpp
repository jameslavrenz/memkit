#pragma once

#include "../status.hpp"
#include "element_policy.hpp"
#include "utility.hpp"

#include <atomic>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

namespace memkit::detail {

enum class spsc_policy : std::uint8_t {
    none              = 0u,
    drop_on_full      = 1u << 0u,
    overwrite_on_full = 1u << 1u,
};

template<typename Policy>
class spsc_queue_core {
public:
    spsc_queue_core() = default;

    [[nodiscard]] status init(
        Policy policy,
        std::byte* storage,
        std::size_t capacity_pow2,
        spsc_policy queue_policy = spsc_policy::none
    ) noexcept
    {
        if (storage == nullptr || capacity_pow2 < 2u || !is_power_of_two(capacity_pow2)) {
            return status::invalid;
        }

        policy_        = policy;
        storage_       = storage;
        capacity_      = capacity_pow2;
        mask_          = capacity_pow2 - 1u;
        queue_policy_  = queue_policy;
        head_.store(0u, std::memory_order_relaxed);
        tail_.store(0u, std::memory_order_relaxed);
        return status::ok;
    }

    void reset_state() noexcept
    {
        policy_   = Policy{};
        storage_  = nullptr;
        capacity_ = 0u;
        mask_     = 0u;
        head_.store(0u, std::memory_order_relaxed);
        tail_.store(0u, std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    [[nodiscard]] std::size_t size() const noexcept
    {
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        const std::size_t head = head_.load(std::memory_order_acquire);
        return tail - head;
    }

    [[nodiscard]] bool empty() const noexcept { return size() == 0u; }

    [[nodiscard]] bool full() const noexcept { return size() >= capacity_; }

    [[nodiscard]] status push(const void* elem) noexcept
    {
        if (elem == nullptr) {
            return status::null_ptr;
        }

        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t head = head_.load(std::memory_order_acquire);

        if (tail - head >= capacity_) {
            if ((static_cast<std::uint8_t>(queue_policy_) &
                 static_cast<std::uint8_t>(spsc_policy::overwrite_on_full)) != 0u) {
                void* dst = slot_at(head & mask_);
                policy_.destroy(dst);
                policy_.move_construct(dst, const_cast<void*>(elem));
                head_.store(head + 1u, std::memory_order_release);
                tail_.store(tail + 1u, std::memory_order_release);
                return status::ok;
            }
            return status::full;
        }

        policy_.copy_construct(slot_at(tail & mask_), elem);
        tail_.store(tail + 1u, std::memory_order_release);
        return status::ok;
    }

    [[nodiscard]] status pop(void* out, bool move_out = true) noexcept
    {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t tail = tail_.load(std::memory_order_acquire);

        if (head == tail) {
            return status::empty;
        }

        void* src = slot_at(head & mask_);
        if (out != nullptr) {
            if (move_out) {
                policy_.move_construct(out, src);
                policy_.destroy(src);
            } else {
                policy_.copy_construct(out, src);
                policy_.destroy(src);
            }
        } else {
            policy_.destroy(src);
        }

        head_.store(head + 1u, std::memory_order_release);
        return status::ok;
    }

    [[nodiscard]] status peek(void* out) const noexcept
    {
        if (out == nullptr) {
            return status::null_ptr;
        }

        const std::size_t head = head_.load(std::memory_order_acquire);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        if (head == tail) {
            return status::empty;
        }

        policy_.copy_construct(out, slot_at(head & mask_));
        return status::ok;
    }

    void clear() noexcept
    {
        while (pop(nullptr, true) == status::ok) {
        }
        head_.store(0u, std::memory_order_release);
        tail_.store(0u, std::memory_order_release);
    }

private:
    [[nodiscard]] void* slot_at(std::size_t index) const noexcept
    {
        return storage_ + (index * policy_.elem_size());
    }

    Policy                   policy_{};
    std::byte*               storage_      = nullptr;
    std::size_t              capacity_     = 0u;
    std::size_t              mask_         = 0u;
    spsc_policy              queue_policy_ = spsc_policy::none;
    std::atomic<std::size_t> head_{0u};
    std::atomic<std::size_t> tail_{0u};
};

} // namespace memkit::detail
