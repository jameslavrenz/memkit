#pragma once

#include "../detail/element_policy.hpp"
#include "../detail/mpsc_queue_core.hpp"
#include "../detail/utility.hpp"
#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace memkit {

/** Bounded multi-producer single-consumer queue (power-of-2 capacity, Vyukov-style). */
template<typename T>
class MpscQueue {
public:
    MpscQueue() noexcept = default;

    MpscQueue(MpscQueue&& other) noexcept
        : core_{std::move(other.core_)}
    {
        other.core_.reset_state();
    }

    MpscQueue& operator=(MpscQueue&& other) noexcept
    {
        if (this != &other) {
            clear();
            core_ = std::move(other.core_);
            other.core_.reset_state();
        }
        return *this;
    }

    MpscQueue(const MpscQueue&)            = delete;
    MpscQueue& operator=(const MpscQueue&) = delete;

    ~MpscQueue() { clear(); core_.destroy_cells(); }

    [[nodiscard]] static constexpr std::size_t cell_stride_bytes() noexcept
    {
        return detail::align_up(
            sizeof(std::atomic<std::size_t>) + sizeof(T),
            alignof(std::atomic<std::size_t>)
        );
    }

    [[nodiscard]] static constexpr std::size_t storage_bytes(std::size_t capacity_pow2) noexcept
    {
        return cell_stride_bytes() * capacity_pow2;
    }

    template<std::size_t CapacityPow2>
    [[nodiscard]] static constexpr std::size_t storage_bytes() noexcept
    {
        return storage_bytes(CapacityPow2);
    }

    [[nodiscard]] status init(std::byte* storage, std::size_t capacity_pow2) noexcept
    {
        if (storage == nullptr || capacity_pow2 < 2u ||
            !detail::is_power_of_two(capacity_pow2)) {
            return status::invalid;
        }

        if ((reinterpret_cast<std::uintptr_t>(storage) % alignof(std::max_align_t)) != 0u) {
            return status::invalid;
        }

        detail::typed_element_policy<T> policy{};
        return core_.init(policy, storage, capacity_pow2);
    }

    template<std::size_t CapacityPow2>
    [[nodiscard]] status init(stl::array<std::byte, storage_bytes<CapacityPow2>()>& storage) noexcept
    {
        return init(storage.data(), CapacityPow2);
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(Arena& arena, std::size_t capacity_pow2)
    {
        if (capacity_pow2 < 2u || !detail::is_power_of_two(capacity_pow2)) {
            return status::invalid;
        }

        void* ptr = nullptr;
        const status st = arena.allocate(storage_bytes(capacity_pow2), alignof(std::max_align_t), &ptr);
        if (!ok(st)) {
            return st;
        }

        return init(static_cast<std::byte*>(ptr), capacity_pow2);
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }
    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status push(const T& value, std::size_t max_spins = 1024u)
    {
        return core_.push(&value, max_spins);
    }

    [[nodiscard]] status pop(T& out, std::size_t max_spins = 1024u)
    {
        return core_.pop(&out, true, max_spins);
    }

private:
    detail::mpsc_queue_core<detail::typed_element_policy<T>> core_{};
};

} // namespace memkit
