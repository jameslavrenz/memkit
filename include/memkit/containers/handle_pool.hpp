#pragma once

#include "../detail/element_policy.hpp"
#include "../detail/handle_pool_core.hpp"
#include "../status.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace memkit {

using handle_t = std::uint32_t;
inline constexpr handle_t invalid_handle = detail::handle_pool_core::invalid_handle;

template<typename T>
class HandlePool {
public:
    HandlePool() noexcept = default;

    HandlePool(HandlePool&& other) noexcept
        : core_{std::move(other.core_)}
        , policy_{}
        , owns_meta_{std::exchange(other.owns_meta_, false)}
        , owns_storage_{std::exchange(other.owns_storage_, false)}
    {}

    HandlePool& operator=(HandlePool&& other) noexcept
    {
        if (this != &other) {
            release_owned();
            core_          = std::move(other.core_);
            owns_meta_     = std::exchange(other.owns_meta_, false);
            owns_storage_  = std::exchange(other.owns_storage_, false);
            other.core_.reset_state();
        }
        return *this;
    }

    HandlePool(const HandlePool&)            = delete;
    HandlePool& operator=(const HandlePool&) = delete;

    ~HandlePool() { release_owned(); }

    [[nodiscard]] static constexpr std::size_t generations_bytes(std::size_t capacity) noexcept
    {
        return detail::handle_pool_core::generations_bytes(capacity);
    }

    [[nodiscard]] static constexpr std::size_t free_stack_bytes(std::size_t capacity) noexcept
    {
        return detail::handle_pool_core::free_stack_bytes(capacity);
    }

    [[nodiscard]] static constexpr std::size_t storage_bytes(std::size_t capacity) noexcept
    {
        return sizeof(T) * capacity;
    }

    [[nodiscard]] status init(
        std::byte* storage,
        std::uint16_t* generations,
        std::uint32_t* free_stack,
        std::size_t capacity
    ) noexcept
    {
        return core_.init(
            storage,
            generations,
            free_stack,
            capacity,
            sizeof(T),
            alignof(T)
        );
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(Arena& arena, std::size_t capacity)
    {
        if (capacity == 0u) {
            return status::invalid;
        }

        void* storage_ptr = nullptr;
        status st = arena.allocate(storage_bytes(capacity), alignof(T), &storage_ptr);
        if (!ok(st)) {
            return st;
        }

        void* generations_ptr = nullptr;
        st = arena.allocate(generations_bytes(capacity), alignof(std::uint16_t), &generations_ptr);
        if (!ok(st)) {
            return st;
        }

        void* free_stack_ptr = nullptr;
        st = arena.allocate(free_stack_bytes(capacity), alignof(std::uint32_t), &free_stack_ptr);
        if (!ok(st)) {
            return st;
        }

        owns_storage_ = true;
        owns_meta_      = true;
        core_.set_storage_kind(
            detail::handle_pool_storage_kind::owns | detail::handle_pool_storage_kind::arena
        );

        return core_.init(
            static_cast<std::byte*>(storage_ptr),
            static_cast<std::uint16_t*>(generations_ptr),
            static_cast<std::uint32_t*>(free_stack_ptr),
            capacity,
            sizeof(T),
            alignof(T)
        );
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] std::size_t available() const noexcept { return core_.available(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }
    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    [[nodiscard]] status acquire(T*& out_ptr, handle_t& out_handle) noexcept
    {
        void* raw = nullptr;
        const status st = core_.acquire(&raw, &out_handle);
        if (!ok(st)) {
            return st;
        }
        out_ptr = static_cast<T*>(raw);
        return status::ok;
    }

    [[nodiscard]] status release(handle_t handle) noexcept
    {
        void* raw = nullptr;
        const status st = core_.get(handle, &raw);
        if (!ok(st)) {
            return st;
        }

        if constexpr (!std::is_trivially_destructible_v<T>) {
            static_cast<T*>(raw)->~T();
        }

        return core_.release(handle);
    }

    [[nodiscard]] status get(handle_t handle, T*& out_ptr) const noexcept
    {
        void* raw = nullptr;
        const status st = core_.get(handle, &raw);
        if (!ok(st)) {
            return st;
        }
        out_ptr = static_cast<T*>(raw);
        return status::ok;
    }

    [[nodiscard]] bool valid(handle_t handle) const noexcept { return core_.valid(handle); }

    template<typename... Args>
    [[nodiscard]] status emplace(handle_t& out_handle, Args&&... args)
    {
        T* slot = nullptr;
        const status st = acquire(slot, out_handle);
        if (!ok(st)) {
            return st;
        }

        new (slot) T(std::forward<Args>(args)...);
        return status::ok;
    }

private:
    void release_owned() noexcept
    {
        if (!owns_storage_ && !owns_meta_) {
            core_.reset_state();
            return;
        }

#if MEMKIT_ALLOW_HEAP
        if ((static_cast<std::uint8_t>(core_.storage_kind()) &
             static_cast<std::uint8_t>(detail::handle_pool_storage_kind::heap)) != 0u) {
            // arena-backed pools are not freed here
        }
#endif

        core_.reset_state();
        owns_storage_ = false;
        owns_meta_    = false;
    }

    detail::handle_pool_core           core_{};
    detail::typed_element_policy<T>    policy_{};
    bool                               owns_meta_    = false;
    bool                               owns_storage_ = false;
};

} // namespace memkit
