#pragma once

#include "../detail/element_policy.hpp"
#include "../detail/spsc_queue_core.hpp"
#include "../detail/utility.hpp"
#include "../status.hpp"
#include "../stl.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace memkit {

enum class spsc_queue_policy : unsigned {
    none              = 0u,
    drop_on_full      = 1u << 0u,
    overwrite_on_full = 1u << 1u,
};

/** Single-producer single-consumer lock-free queue (power-of-2 capacity). */
template<typename T>
class SpscQueue {
public:
    SpscQueue() noexcept = default;

    SpscQueue(SpscQueue&& other) noexcept
        : core_{std::move(other.core_)}
        , policy_{other.policy_}
    {
        other.core_.reset_state();
    }

    SpscQueue& operator=(SpscQueue&& other) noexcept
    {
        if (this != &other) {
            clear();
            core_    = std::move(other.core_);
            policy_  = other.policy_;
            other.core_.reset_state();
        }
        return *this;
    }

    SpscQueue(const SpscQueue&)            = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;

    ~SpscQueue() { clear(); }

    [[nodiscard]] static constexpr std::size_t storage_bytes(std::size_t capacity_pow2) noexcept
    {
        return capacity_pow2 * sizeof(T);
    }

    template<std::size_t CapacityPow2>
    [[nodiscard]] static constexpr std::size_t storage_bytes() noexcept
    {
        return CapacityPow2 * sizeof(T);
    }

    [[nodiscard]] status init(
        std::byte* storage,
        std::size_t capacity_pow2,
        spsc_queue_policy policy = spsc_queue_policy::none
    ) noexcept
    {
        if (storage == nullptr || capacity_pow2 < 2u ||
            !detail::is_power_of_two(capacity_pow2)) {
            return status::invalid;
        }

        detail::typed_element_policy<T> elem_policy{};
        return core_.init(
            elem_policy,
            storage,
            capacity_pow2,
            to_detail_policy(policy)
        );
    }

    template<std::size_t CapacityPow2>
    [[nodiscard]] status init(
        stl::array<std::byte, storage_bytes(CapacityPow2)>& storage,
        spsc_queue_policy policy = spsc_queue_policy::none
    ) noexcept
    {
        return init(storage.data(), CapacityPow2, policy);
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(
        Arena& arena,
        std::size_t capacity_pow2,
        spsc_queue_policy policy = spsc_queue_policy::none
    )
    {
        if (capacity_pow2 < 2u || !detail::is_power_of_two(capacity_pow2)) {
            return status::invalid;
        }

        void* ptr = nullptr;
        const status st = arena.allocate(storage_bytes(capacity_pow2), alignof(T), &ptr);
        if (!ok(st)) {
            return st;
        }

        return init(static_cast<std::byte*>(ptr), capacity_pow2, policy);
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }
    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status push(const T& value) { return core_.push(&value); }
    [[nodiscard]] status push(T&& value) { return core_.push(&value); }

    [[nodiscard]] status pop(T& out) { return core_.pop(&out, true); }
    [[nodiscard]] status peek(T& out) const { return core_.peek(&out); }

private:
    [[nodiscard]] static detail::spsc_policy to_detail_policy(spsc_queue_policy policy) noexcept
    {
        if ((static_cast<unsigned>(policy) &
             static_cast<unsigned>(spsc_queue_policy::overwrite_on_full)) != 0u) {
            return detail::spsc_policy::overwrite_on_full;
        }
        if ((static_cast<unsigned>(policy) &
             static_cast<unsigned>(spsc_queue_policy::drop_on_full)) != 0u) {
            return detail::spsc_policy::drop_on_full;
        }
        return detail::spsc_policy::none;
    }

    detail::spsc_queue_core<detail::typed_element_policy<T>> core_{};
    spsc_queue_policy                                        policy_ = spsc_queue_policy::none;
};

} // namespace memkit
