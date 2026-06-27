#pragma once

#include "../config.hpp"
#include "../detail/compare_policy.hpp"
#include "../detail/element_policy.hpp"
#include "../detail/pqueue_core.hpp"
#include "../detail/utility.hpp"
#include "../status.hpp"
#include "../stl.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

namespace memkit {

enum class pqueue_policy : unsigned {
    none     = 0u,
    growable = 1u << 0u,
};

template<typename T, typename Compare = stl::less<T>>
class PQueue {
public:
    PQueue() noexcept = default;

    explicit PQueue(Compare compare) noexcept
        : compare_{std::move(compare)}
    {}

    PQueue(PQueue&& other) noexcept
        : core_{std::move(other.core_)}
        , compare_{std::move(other.compare_)}
        , owns_storage_{std::exchange(other.owns_storage_, false)}
    {}

    PQueue& operator=(PQueue&& other) noexcept
    {
        if (this != &other) {
            clear();
            release_storage();
            core_         = std::move(other.core_);
            compare_      = std::move(other.compare_);
            owns_storage_ = std::exchange(other.owns_storage_, false);
        }
        return *this;
    }

    PQueue(const PQueue&)            = delete;
    PQueue& operator=(const PQueue&) = delete;

    ~PQueue() { clear(); release_storage(); }

    [[nodiscard]] status init(
        std::byte* storage,
        std::size_t capacity,
        pqueue_policy policy = pqueue_policy::none,
        Compare compare = Compare{}
    ) noexcept
    {
        detail::typed_element_policy<T> ep{};
        compare_ = std::move(compare);
        return core_.init(
            ep,
            compare_,
            storage,
            capacity,
            static_cast<detail::pqueue_policy>(policy)
        );
    }

    template<typename Arena>
    [[nodiscard]] status init_from_arena(
        Arena& arena,
        std::size_t initial_capacity,
        pqueue_policy policy = pqueue_policy::growable,
        Compare compare = Compare{}
    )
    {
        if (initial_capacity == 0u) {
            initial_capacity = 1u;
        }

        void* ptr = nullptr;
        const status st = arena.allocate(initial_capacity * sizeof(T), alignof(T), &ptr);
        if (!ok(st)) {
            return st;
        }

        compare_ = std::move(compare);
        detail::typed_element_policy<T> ep{};
        return core_.init(
            ep,
            compare_,
            static_cast<std::byte*>(ptr),
            initial_capacity,
            static_cast<detail::pqueue_policy>(policy)
        );
    }

    [[nodiscard]] std::size_t size() const noexcept { return core_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return core_.capacity(); }
    [[nodiscard]] bool empty() const noexcept { return core_.empty(); }

    [[nodiscard]] bool full() const noexcept { return core_.full(); }

    void clear() noexcept { core_.clear(); }

    [[nodiscard]] status reserve(std::size_t min_capacity)
    {
        if (min_capacity <= core_.capacity()) {
            return status::ok;
        }
        if ((static_cast<unsigned>(core_.flags()) &
             static_cast<unsigned>(detail::pqueue_policy::growable)) == 0u) {
            return status::full;
        }
        return reallocate(grow_capacity(core_.capacity(), min_capacity));
    }

    [[nodiscard]] status push(const T& value) { return emplace(value); }
    [[nodiscard]] status push(T&& value) { return emplace(std::move(value)); }

    template<typename... Args>
    [[nodiscard]] status emplace(Args&&... args)
    {
        T value(std::forward<Args>(args)...);
        const status grow_st = ensure_capacity(core_.size() + 1u);
        if (!ok(grow_st)) {
            return grow_st;
        }
        return core_.push(&value);
    }

    [[nodiscard]] status pop(T* out = nullptr)
    {
        return core_.pop(out != nullptr ? static_cast<void*>(out) : nullptr);
    }

    [[nodiscard]] status peek(T& out) const
    {
        return core_.peek(static_cast<void*>(&out));
    }

    [[nodiscard]] T* top() noexcept
    {
        return static_cast<T*>(core_.top());
    }

    [[nodiscard]] const T* top() const noexcept
    {
        return static_cast<const T*>(core_.top());
    }

    template<typename Visitor>
    [[nodiscard]] status for_each(Visitor&& visit) const
    {
        return core_.for_each([&visit](const void* slot, std::size_t index) {
            if constexpr (std::is_same_v<
                              std::invoke_result_t<Visitor, const T&, std::size_t>,
                              status>) {
                return visit(*static_cast<const T*>(slot), index);
            }
            visit(*static_cast<const T*>(slot), index);
            return status::ok;
        });
    }

private:
    using core_type = detail::pqueue_core<detail::typed_element_policy<T>, Compare, T>;

    [[nodiscard]] static std::size_t grow_capacity(std::size_t current, std::size_t required)
    {
        std::size_t new_capacity = current > 0u ? current : 1u;
        while (new_capacity < required) {
            if (new_capacity > SIZE_MAX / 2u) {
                return required;
            }
            new_capacity *= 2u;
        }
        return new_capacity;
    }

    [[nodiscard]] status ensure_capacity(std::size_t min_capacity)
    {
        if (core_.capacity() >= min_capacity) {
            return status::ok;
        }
        if ((static_cast<unsigned>(core_.flags()) &
             static_cast<unsigned>(detail::pqueue_policy::growable)) == 0u) {
            return status::full;
        }
        const std::size_t new_capacity = grow_capacity(core_.capacity(), min_capacity);
        if (new_capacity < min_capacity) {
            return status::oom;
        }
        return reallocate(new_capacity);
    }

    [[nodiscard]] status reallocate(std::size_t new_capacity)
    {
#if MEMKIT_ALLOW_HEAP
        void* const new_storage = std::realloc(core_.storage(), new_capacity * sizeof(T));
        if (new_storage == nullptr) {
            return status::oom;
        }
        core_.set_storage(static_cast<std::byte*>(new_storage), new_capacity);
        core_.set_storage_kind(detail::pqueue_storage_kind::owns | detail::pqueue_storage_kind::heap);
        owns_storage_ = true;
        return status::ok;
#else
        (void)new_capacity;
        return status::unsupported;
#endif
    }

    void release_storage() noexcept
    {
        if (owns_storage_ && core_.storage() != nullptr) {
#if MEMKIT_ALLOW_HEAP
            std::free(core_.storage());
#endif
        }
        owns_storage_ = false;
    }

    core_type core_{};
    Compare   compare_{};
    bool      owns_storage_ = false;
};

} // namespace memkit
