#pragma once

#include "../config.hpp"
#include "../detail/element_policy.hpp"
#include "../detail/list_core.hpp"
#include "../detail/utility.hpp"
#include "../status.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

namespace memkit {

enum class list_policy : unsigned {
    none         = 0u,
    fixed_pool   = 1u << 0u,
    heap_dynamic = 1u << 1u,
};

template<typename T>
class List {
public:
    struct node {
        node* next = nullptr;
        alignas(T) std::byte storage[sizeof(T)];

        [[nodiscard]] T& value() noexcept { return *reinterpret_cast<T*>(storage); }
        [[nodiscard]] const T& value() const noexcept { return *reinterpret_cast<const T*>(storage); }
    };

    List() noexcept = default;

    List(List&& other) noexcept
        : core_{std::move(other.core_)}
        , owns_pool_{std::exchange(other.owns_pool_, false)}
    {}

    List& operator=(List&& other) noexcept
    {
        if (this != &other) {
            clear();
            release_pool();
            core_      = std::move(other.core_);
            owns_pool_ = std::exchange(other.owns_pool_, false);
        }
        return *this;
    }

    List(const List&)            = delete;
    List& operator=(const List&) = delete;

    ~List() { clear(); release_pool(); }

    [[nodiscard]] static constexpr std::size_t node_stride() noexcept
    {
        return detail::list_core<detail::typed_element_policy<T>>::node_stride(sizeof(T));
    }

    [[nodiscard]] static constexpr std::size_t pool_bytes(std::size_t node_capacity) noexcept
    {
        return detail::list_core<detail::typed_element_policy<T>>::pool_bytes(
            node_capacity,
            sizeof(T)
        );
    }

    [[nodiscard]] status init(std::byte* node_pool, std::size_t node_capacity) noexcept
    {
        detail::typed_element_policy<T> policy{};
        return core_.init(
            policy,
            node_pool,
            node_capacity,
            detail::list_policy::fixed_pool
        );
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(Arena& arena, std::size_t node_capacity)
    {
        if (node_capacity == 0u) {
            return status::invalid;
        }

        void* ptr = nullptr;
        const status st = arena.allocate(pool_bytes(node_capacity), alignof(node), &ptr);
        if (!ok(st)) {
            return st;
        }

        detail::typed_element_policy<T> policy{};
        return core_.init(
            policy,
            static_cast<std::byte*>(ptr),
            node_capacity,
            detail::list_policy::fixed_pool
        );
    }

    [[nodiscard]] status init_dynamic() noexcept
    {
        detail::typed_element_policy<T> policy{};
        return core_.init_dynamic(policy);
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }
    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status push_front(const T& value) { return core_.push_front(&value); }
    [[nodiscard]] status push_front(T&& value) { return core_.push_front(&value); }
    [[nodiscard]] status push_back(const T& value) { return core_.push_back(&value); }
    [[nodiscard]] status push_back(T&& value) { return core_.push_back(&value); }

    [[nodiscard]] status pop_front(T* out = nullptr)
    {
        return core_.pop_front(out != nullptr ? static_cast<void*>(out) : nullptr);
    }

    [[nodiscard]] status pop_back(T* out = nullptr)
    {
        return core_.pop_back(out != nullptr ? static_cast<void*>(out) : nullptr);
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

    [[nodiscard]] status insert_at(std::size_t index, const T& value)
    {
        return core_.insert_at(index, &value);
    }

    [[nodiscard]] status insert_at(std::size_t index, T&& value)
    {
        return core_.insert_at(index, &value);
    }

    [[nodiscard]] status remove_at(std::size_t index, T* out = nullptr)
    {
        return core_.remove_at(index, out != nullptr ? static_cast<void*>(out) : nullptr);
    }

    template<typename Predicate>
    [[nodiscard]] status remove_first(Predicate&& pred, T* out = nullptr)
    {
        return core_.remove_first(
            [&pred](const void* elem) { return pred(*static_cast<const T*>(elem)); },
            out != nullptr ? static_cast<void*>(out) : nullptr
        );
    }

    [[nodiscard]] T* front() noexcept { return static_cast<T*>(core_.front()); }
    [[nodiscard]] const T* front() const noexcept { return static_cast<const T*>(core_.front()); }

    template<typename Visitor>
    [[nodiscard]] status for_each(Visitor&& visit) const
    {
        return core_.for_each([&visit](const void* elem, std::size_t index) {
            if constexpr (std::is_same_v<
                              std::invoke_result_t<Visitor, const T&, std::size_t>,
                              status>) {
                return visit(*static_cast<const T*>(elem), index);
            }
            visit(*static_cast<const T*>(elem), index);
            return status::ok;
        });
    }

private:
    void release_pool() noexcept
    {
        if (owns_pool_ && core_.node_pool() != nullptr) {
#if MEMKIT_ALLOW_HEAP
            std::free(core_.node_pool());
#endif
        }
        owns_pool_ = false;
    }

    detail::list_core<detail::typed_element_policy<T>> core_{};
    bool                                               owns_pool_ = false;
};

} // namespace memkit
