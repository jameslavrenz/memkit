#pragma once

#include "../detail/double_buffer_core.hpp"
#include "../detail/element_policy.hpp"
#include "../status.hpp"
#include "../stl.hpp"

#include <atomic>
#include <cstddef>
#include <utility>

namespace memkit {

/** Ping-pong double buffer for DMA/ADC/audio (producer fills, consumer reads, then swap). */
template<typename T>
class DoubleBuffer {
public:
    DoubleBuffer() noexcept = default;

    DoubleBuffer(DoubleBuffer&& other) noexcept
        : core_{std::move(other.core_)}
    {
        other.core_.reset_state();
    }

    DoubleBuffer& operator=(DoubleBuffer&& other) noexcept
    {
        if (this != &other) {
            core_ = std::move(other.core_);
            other.core_.reset_state();
        }
        return *this;
    }

    DoubleBuffer(const DoubleBuffer&)            = delete;
    DoubleBuffer& operator=(const DoubleBuffer&) = delete;

    [[nodiscard]] static constexpr std::size_t storage_bytes(std::size_t slot_capacity) noexcept
    {
        return slot_capacity * sizeof(T) * 2u;
    }

    template<std::size_t SlotCapacity>
    [[nodiscard]] static constexpr std::size_t storage_bytes() noexcept
    {
        return storage_bytes(SlotCapacity);
    }

    [[nodiscard]] status init(std::byte* storage, std::size_t slot_capacity) noexcept
    {
        if (storage == nullptr || slot_capacity == 0u) {
            return status::invalid;
        }

        detail::typed_element_policy<T> policy{};
        return core_.init(policy, storage, slot_capacity);
    }

    template<std::size_t SlotCapacity>
    [[nodiscard]] status init(stl::array<std::byte, storage_bytes<SlotCapacity>()>& storage) noexcept
    {
        return init(storage.data(), SlotCapacity);
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(Arena& arena, std::size_t slot_capacity)
    {
        if (slot_capacity == 0u) {
            return status::invalid;
        }

        void* ptr = nullptr;
        const status st = arena.allocate(storage_bytes(slot_capacity), alignof(T), &ptr);
        if (!ok(st)) {
            return st;
        }

        return init(static_cast<std::byte*>(ptr), slot_capacity);
    }

    [[nodiscard]] std::size_t slot_capacity() const noexcept { return core_.slot_capacity(); }

    [[nodiscard]] stl::span<T> write_span() noexcept
    {
        return stl::span<T>(
            static_cast<T*>(core_.write_slot()),
            core_.slot_capacity()
        );
    }

    [[nodiscard]] stl::span<const T> read_span() const noexcept
    {
        return stl::span<const T>(
            static_cast<const T*>(core_.read_slot()),
            core_.slot_capacity()
        );
    }

    void publish() noexcept { core_.publish(); }

private:
    detail::double_buffer_core<detail::typed_element_policy<T>> core_{};
};

} // namespace memkit
