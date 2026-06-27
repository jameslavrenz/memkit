#pragma once

#include "../detail/element_policy.hpp"
#include "../detail/vector_core.hpp"
#include "../detail/storage_view.hpp"
#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace memkit {

enum class stack_policy : unsigned {
    none     = 0u,
    growable = 1u << 0u,
};

template<typename T>
class Stack {
public:
    Stack() noexcept = default;

    Stack(Stack&& other) noexcept
        : core_{std::move(other.core_)}
        , arena_{std::move(other.arena_)}
    {}

    Stack& operator=(Stack&& other) noexcept
    {
        if (this != &other) {
            clear();
            core_  = std::move(other.core_);
            arena_ = std::move(other.arena_);
        }
        return *this;
    }

    Stack(const Stack&)            = delete;
    Stack& operator=(const Stack&) = delete;

    ~Stack() { clear(); }

    [[nodiscard]] status init(
        std::byte* storage,
        std::size_t capacity,
        stack_policy policy = stack_policy::none
    ) noexcept
    {
        detail::typed_element_policy<T> ep{};
        return core_.init(ep, storage, capacity, to_growable_policy(policy));
    }

    [[nodiscard]] status init(
        stl::byte_span storage,
        std::size_t capacity,
        stack_policy policy = stack_policy::none
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
        stack_policy policy = stack_policy::none
    ) noexcept
    {
        return init(stl::span<T>{storage.data(), N}, policy);
    }

    [[nodiscard]] status init(
        stl::span<T> storage,
        stack_policy policy = stack_policy::none
    ) noexcept
    {
        return init(detail::object_bytes(storage), storage.size(), policy);
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(
        Arena& arena,
        std::size_t initial_capacity,
        stack_policy policy = stack_policy::growable
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
            to_growable_policy(policy)
        );
        if (!ok(init_st)) {
            return init_st;
        }

        core_.set_storage_kind(detail::linear_storage_kind::owns | detail::linear_storage_kind::arena);
        arena_.bind(arena);
        bind_grow_alloc();
        return status::ok;
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }

    [[nodiscard]] bool full() const noexcept
    {
        if (detail::has(core_.grow_flags(), detail::growable_policy::growable)) {
            return false;
        }
        return core_.size() >= core_.capacity();
    }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status push(const T& value) { return core_.push_back(&value, false); }
    [[nodiscard]] status push(T&& value) { return core_.push_back(&value, true); }

    template<typename... Args>
    [[nodiscard]] status emplace(Args&&... args)
    {
        T value(std::forward<Args>(args)...);
        return push(std::move(value));
    }

    [[nodiscard]] status pop(T* out = nullptr)
    {
        return core_.pop_back(out != nullptr ? static_cast<void*>(out) : nullptr);
    }

    [[nodiscard]] status peek(T& out) const
    {
        return core_.peek_back(static_cast<void*>(&out));
    }

    [[nodiscard]] T* top() noexcept
    {
        return core_.empty() ? nullptr : static_cast<T*>(core_.at(core_.size() - 1u));
    }

    [[nodiscard]] const T* top() const noexcept
    {
        return core_.empty() ? nullptr : static_cast<const T*>(core_.at(core_.size() - 1u));
    }

    [[nodiscard]] status reserve(std::size_t min_capacity)
    {
        bind_grow_alloc();
        return core_.reserve(min_capacity);
    }

private:
    [[nodiscard]] static detail::growable_policy to_growable_policy(stack_policy policy) noexcept
    {
        if ((static_cast<unsigned>(policy) &
             static_cast<unsigned>(stack_policy::growable)) != 0u) {
            return detail::growable_policy::growable;
        }
        return detail::growable_policy::none;
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

    detail::vector_core<detail::typed_element_policy<T>> core_{};
    detail::arena_allocator                             arena_{};
};

} // namespace memkit
