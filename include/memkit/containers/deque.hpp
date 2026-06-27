#pragma once

#include "../detail/deque_core.hpp"
#include "../detail/element_policy.hpp"
#include "../detail/storage_view.hpp"
#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace memkit {

enum class deque_policy : unsigned {
    none     = 0u,
    growable = 1u << 0u,
};

template<typename T>
class Deque {
public:
    Deque() noexcept = default;

    Deque(Deque&& other) noexcept
        : core_{std::move(other.core_)}
        , arena_{std::move(other.arena_)}
    {}

    Deque& operator=(Deque&& other) noexcept
    {
        if (this != &other) {
            clear();
            core_  = std::move(other.core_);
            arena_ = std::move(other.arena_);
        }
        return *this;
    }

    Deque(const Deque&)            = delete;
    Deque& operator=(const Deque&) = delete;

    ~Deque() { clear(); }

    [[nodiscard]] status init(
        std::byte* storage,
        std::size_t capacity,
        deque_policy policy = deque_policy::none
    ) noexcept
    {
        detail::typed_element_policy<T> ep{};
        return core_.init(ep, storage, capacity, to_buffer_policy(policy));
    }

    [[nodiscard]] status init(
        stl::byte_span storage,
        std::size_t capacity,
        deque_policy policy = deque_policy::none
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
        deque_policy policy = deque_policy::none
    ) noexcept
    {
        return init(stl::span<T>{storage.data(), N}, policy);
    }

    [[nodiscard]] status init(
        stl::span<T> storage,
        deque_policy policy = deque_policy::none
    ) noexcept
    {
        return init(detail::object_bytes(storage), storage.size(), policy);
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(
        Arena& arena,
        std::size_t initial_capacity,
        deque_policy policy = deque_policy::growable
    )
    {
        if (initial_capacity == 0u) {
            return status::invalid;
        }

        void* ptr = nullptr;
        const status st = arena.allocate(
            initial_capacity * sizeof(T),
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
            initial_capacity,
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

    [[nodiscard]] status push_front(const T& value) { return core_.push_front(&value, false); }
    [[nodiscard]] status push_front(T&& value) { return core_.push_front(&value, true); }

    [[nodiscard]] status pop_front(T* out = nullptr)
    {
        return core_.pop_front(
            out != nullptr ? static_cast<void*>(out) : nullptr,
            true
        );
    }

    [[nodiscard]] status pop_back(T* out = nullptr)
    {
        return core_.pop_back(
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

    [[nodiscard]] T* front() noexcept
    {
        return static_cast<T*>(core_.front());
    }

    [[nodiscard]] const T* front() const noexcept
    {
        return static_cast<const T*>(core_.front());
    }

    [[nodiscard]] T* back() noexcept
    {
        return static_cast<T*>(core_.back());
    }

    [[nodiscard]] const T* back() const noexcept
    {
        return static_cast<const T*>(core_.back());
    }

    [[nodiscard]] status reserve(std::size_t min_capacity)
    {
        bind_grow_alloc();
        return core_.reserve(min_capacity);
    }

private:
    [[nodiscard]] static detail::ring_buffer_policy to_buffer_policy(deque_policy policy) noexcept
    {
        if ((static_cast<unsigned>(policy) &
             static_cast<unsigned>(deque_policy::growable)) != 0u) {
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

    detail::deque_core<detail::typed_element_policy<T>> core_{};
    detail::arena_allocator                             arena_{};
};

} // namespace memkit
