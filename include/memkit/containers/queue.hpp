#pragma once

#include "../detail/element_policy.hpp"
#include "../detail/queue_core.hpp"
#include "../detail/storage_view.hpp"
#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace memkit {

enum class queue_policy : unsigned {
    none     = 0u,
    growable = 1u << 0u,
};

template<typename T>
class Queue {
public:
    Queue() noexcept = default;

    Queue(Queue&& other) noexcept
        : core_{std::move(other.core_)}
        , arena_{std::move(other.arena_)}
    {}

    Queue& operator=(Queue&& other) noexcept
    {
        if (this != &other) {
            clear();
            core_  = std::move(other.core_);
            arena_ = std::move(other.arena_);
        }
        return *this;
    }

    Queue(const Queue&)            = delete;
    Queue& operator=(const Queue&) = delete;

    ~Queue() { clear(); }

    [[nodiscard]] status init(
        std::byte* storage,
        std::size_t capacity,
        queue_policy policy = queue_policy::none
    ) noexcept
    {
        detail::typed_element_policy<T> ep{};
        return core_.init(ep, storage, capacity, to_buffer_policy(policy));
    }

    [[nodiscard]] status init(
        stl::byte_span storage,
        std::size_t capacity,
        queue_policy policy = queue_policy::none
    ) noexcept
    {
        if (!detail::byte_span_fits(storage, sizeof(T), capacity)) {
            return status::invalid;
        }
        return init(storage.data(), capacity, policy);
    }

    template<std::size_t N>
    [[nodiscard]] status init(
        stl::array<T, N>& storage,
        queue_policy policy = queue_policy::none
    ) noexcept
    {
        return init(stl::span<T>{storage.data(), N}, policy);
    }

    [[nodiscard]] status init(
        stl::span<T> storage,
        queue_policy policy = queue_policy::none
    ) noexcept
    {
        return init(detail::object_bytes(storage), storage.size(), policy);
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(
        Arena& arena,
        std::size_t capacity,
        queue_policy policy = queue_policy::growable
    )
    {
        if (capacity == 0u) {
            return status::invalid;
        }

        void* ptr = nullptr;
        const status st = arena.allocate(
            sizeof(T) * capacity,
            alignof(T),
            &ptr
        );
        if (!ok(st)) {
            return st;
        }

        detail::typed_element_policy<T> ep{};
        const status init_st = core_.init(
            ep,
            static_cast<std::byte*>(ptr),
            capacity,
            to_buffer_policy(policy)
        );
        if (!ok(init_st)) {
            return init_st;
        }

        core_.set_storage_kind(
            detail::ring_buffer_storage_kind::owns | detail::ring_buffer_storage_kind::arena
        );
        arena_.bind(arena);
        bind_grow_alloc();
        return status::ok;
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }
    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status push_back(const T& value) { return core_.push_back(&value, false); }
    [[nodiscard]] status push_back(T&& value) { return core_.push_back(&value, false, true); }

    [[nodiscard]] status pop_front(T* out = nullptr)
    {
        return core_.pop_front(
            out != nullptr ? static_cast<void*>(out) : nullptr,
            true
        );
    }

    [[nodiscard]] status peek_front(T& out) const
    {
        return core_.peek_front(static_cast<void*>(&out));
    }

    [[nodiscard]] status peek_back(T& out) const
    {
        return core_.peek_back(static_cast<void*>(&out));
    }

    [[nodiscard]] status peek_at(std::size_t index, T& out) const
    {
        return core_.peek_at(index, static_cast<void*>(&out));
    }

    [[nodiscard]] const T* data_at(std::size_t index) const noexcept
    {
        return static_cast<const T*>(core_.logical_slot(index));
    }

    [[nodiscard]] std::size_t readable_contiguous(const T** out_ptr) const noexcept
    {
        const void* ptr = nullptr;
        const std::size_t n = core_.readable_contiguous(&ptr);
        if (out_ptr != nullptr) {
            *out_ptr = n > 0u ? static_cast<const T*>(ptr) : nullptr;
        }
        return n;
    }

    [[nodiscard]] std::size_t writable_contiguous(T** out_ptr) noexcept
    {
        bind_grow_alloc();
        void* ptr = nullptr;
        const std::size_t n = core_.writable_contiguous(&ptr);
        if (out_ptr != nullptr) {
            *out_ptr = n > 0u ? static_cast<T*>(ptr) : nullptr;
        }
        return n;
    }

    void commit_read(std::size_t n) noexcept { core_.commit_read(n); }
    void commit_write(std::size_t n) noexcept { core_.commit_write(n); }

private:
    [[nodiscard]] static detail::ring_buffer_policy to_buffer_policy(queue_policy policy) noexcept
    {
        if ((static_cast<unsigned>(policy) &
             static_cast<unsigned>(queue_policy::growable)) != 0u) {
            return detail::ring_buffer_policy::growable;
        }
        return detail::ring_buffer_policy::none;
    }

    void bind_grow_alloc()
    {
        if (arena_.bound()) {
            core_.set_grow_alloc({
                &arena_,
                +[](void* ctx, std::size_t size, std::size_t align, void** out) -> status {
                    return static_cast<detail::arena_allocator*>(ctx)->allocate(size, align, out);
                },
            });
        }
    }

    detail::queue_core<detail::typed_element_policy<T>> core_{};
    detail::arena_allocator                             arena_{};
};

} // namespace memkit
